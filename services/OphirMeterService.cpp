#include "OphirMeterService.h"

#include <algorithm>
#include <chrono>
#include <cmath>  // for fabs
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <OleAuto.h>
#include <Windows.h>
#include <comdef.h>
#include <objbase.h>
#endif

namespace {
#ifdef _WIN32
struct CoInitGuard {
  HRESULT hr{E_FAIL};
  CoInitGuard() { hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); }
  ~CoInitGuard() {
    if (SUCCEEDED(hr)) CoUninitialize();
  }
};

static std::string hrHex(HRESULT hr) {
  std::ostringstream os;
  os << "0x" << std::hex << std::uppercase << (unsigned long)hr;
  return os.str();
}

static bool getDispId(IDispatch* d, const wchar_t* name, DISPID& id) {
  LPOLESTR n = const_cast<LPOLESTR>(name);
  return SUCCEEDED(d->GetIDsOfNames(IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id));
}

static bool invokeVoid0(IDispatch* d, const wchar_t* name) {
  DISPID id;
  if (!getDispId(d, name, id)) return false;
  DISPPARAMS dp{nullptr, nullptr, 0, 0};
  return SUCCEEDED(d->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD,
                             &dp, nullptr, nullptr, nullptr));
}

// ScanUSB(QVariant& serialNumbers)
static bool invokeScanUSB(IDispatch* d, VARIANT& outSerials) {
  DISPID id;
  if (!getDispId(d, L"ScanUSB", id)) return false;

  VariantInit(&outSerials);

  VARIANTARG args[1];
  VariantInit(&args[0]);
  args[0].vt = VT_VARIANT | VT_BYREF;
  args[0].pvarVal = &outSerials;

  DISPPARAMS dp{args, nullptr, 1, 0};
  return SUCCEEDED(d->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD,
                             &dp, nullptr, nullptr, nullptr));
}

// OpenUSBDevice(QString, int&)
static bool invokeOpenUSBDevice(IDispatch* d, const QString& serial,
                                long& outHandle) {
  DISPID id;
  if (!getDispId(d, L"OpenUSBDevice", id)) return false;

  BSTR b = SysAllocStringLen(reinterpret_cast<const OLECHAR*>(serial.utf16()),
                             (UINT)serial.size());
  outHandle = 0;

  VARIANT vHandle;
  VariantInit(&vHandle);
  vHandle.vt = VT_I4;
  vHandle.lVal = 0;

  VARIANTARG args[2];
  VariantInit(&args[0]);  // second param int& hDevice
  VariantInit(&args[1]);  // first param QString serial

  args[0].vt = VT_I4 | VT_BYREF;
  args[0].plVal = &vHandle.lVal;

  args[1].vt = VT_BSTR;
  args[1].bstrVal = b;

  DISPPARAMS dp{args, nullptr, 2, 0};
  HRESULT hr = d->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD,
                         &dp, nullptr, nullptr, nullptr);

  SysFreeString(b);

  if (FAILED(hr)) return false;
  outHandle = vHandle.lVal;
  return true;
}

// IsSensorExists(int,int,bool&)
static bool invokeIsSensorExists(IDispatch* d, long h, long ch, bool& exists) {
  DISPID id;
  if (!getDispId(d, L"IsSensorExists", id)) return false;

  VARIANT_BOOL vb = VARIANT_FALSE;

  VARIANTARG args[3];
  VariantInit(&args[0]);  // bool&
  VariantInit(&args[1]);  // channel
  VariantInit(&args[2]);  // handle

  args[0].vt = VT_BOOL | VT_BYREF;
  args[0].pboolVal = &vb;

  args[1].vt = VT_I4;
  args[1].lVal = ch;

  args[2].vt = VT_I4;
  args[2].lVal = h;

  DISPPARAMS dp{args, nullptr, 3, 0};
  HRESULT hr = d->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD,
                         &dp, nullptr, nullptr, nullptr);
  if (FAILED(hr)) return false;

  exists = (vb == VARIANT_TRUE);
  return true;
}

static bool invokeStartStop(IDispatch* d, const wchar_t* name, long h,
                            long ch) {
  DISPID id;
  if (!getDispId(d, name, id)) return false;

  VARIANTARG args[2];
  VariantInit(&args[0]);
  VariantInit(&args[1]);

  args[0].vt = VT_I4;
  args[0].lVal = ch;
  args[1].vt = VT_I4;
  args[1].lVal = h;

  DISPPARAMS dp{args, nullptr, 2, 0};
  return SUCCEEDED(d->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD,
                             &dp, nullptr, nullptr, nullptr));
}

// GetData(int,int,QVariant&,QVariant&,QVariant&)
static bool invokeGetData(IDispatch* d, long h, long ch, VARIANT& vValues,
                          VARIANT& vTs, VARIANT& vSt) {
  DISPID id;
  if (!getDispId(d, L"GetData", id)) return false;

  VariantInit(&vValues);
  VariantInit(&vTs);
  VariantInit(&vSt);

  VARIANTARG args[5];
  VariantInit(&args[0]);  // status&
  VariantInit(&args[1]);  // timestamp&
  VariantInit(&args[2]);  // value&
  VariantInit(&args[3]);  // channel
  VariantInit(&args[4]);  // handle

  args[0].vt = VT_VARIANT | VT_BYREF;
  args[0].pvarVal = &vSt;
  args[1].vt = VT_VARIANT | VT_BYREF;
  args[1].pvarVal = &vTs;
  args[2].vt = VT_VARIANT | VT_BYREF;
  args[2].pvarVal = &vValues;
  args[3].vt = VT_I4;
  args[3].lVal = ch;
  args[4].vt = VT_I4;
  args[4].lVal = h;

  DISPPARAMS dp{args, nullptr, 5, 0};
  return SUCCEEDED(d->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD,
                             &dp, nullptr, nullptr, nullptr));
}

static std::vector<QString> bstrArrayToQStrings(const VARIANT& v) {
  std::vector<QString> out;
  if (!(v.vt & VT_ARRAY) || (v.vt & VT_TYPEMASK) != VT_BSTR || !v.parray)
    return out;

  LONG lb = 0, ub = -1;
  if (FAILED(SafeArrayGetLBound(v.parray, 1, &lb))) return out;
  if (FAILED(SafeArrayGetUBound(v.parray, 1, &ub))) return out;

  for (LONG i = lb; i <= ub; ++i) {
    BSTR b = nullptr;
    if (SUCCEEDED(SafeArrayGetElement(v.parray, &i, &b)) && b) {
      out.emplace_back(QString::fromWCharArray(b, SysStringLen(b)));
      SysFreeString(b);
    }
  }
  return out;
}

static std::vector<double> r8ArrayToDoubles(const VARIANT& v) {
  std::vector<double> out;
  if (!(v.vt & VT_ARRAY) || (v.vt & VT_TYPEMASK) != VT_R8 || !v.parray)
    return out;

  LONG lb = 0, ub = -1;
  if (FAILED(SafeArrayGetLBound(v.parray, 1, &lb))) return out;
  if (FAILED(SafeArrayGetUBound(v.parray, 1, &ub))) return out;

  for (LONG i = lb; i <= ub; ++i) {
    double d = 0.0;
    if (SUCCEEDED(SafeArrayGetElement(v.parray, &i, &d))) out.push_back(d);
  }
  return out;
}

// ---------------------- CLSID discovery helpers, adding this to remove
// dependency on just having 1 ophir meter----------------------

// Case-insensitive search helper
static bool containsCaseInsensitive(const std::wstring& hay,
                                    const std::wstring& needle) {
  if (needle.empty()) return true;
  std::wstring hay_l = hay;
  std::wstring needle_l = needle;
  std::transform(hay_l.begin(), hay_l.end(), hay_l.begin(), ::towlower);
  std::transform(needle_l.begin(), needle_l.end(), needle_l.begin(),
                 ::towlower);
  return hay_l.find(needle_l) != std::wstring::npos;
}

// Attempts to read the default string value of an opened key into out (returns
// true on success)
static bool queryDefaultStringValue(HKEY hKey, std::wstring& out) {
  DWORD type = 0;
  DWORD size = 0;
  LONG r = RegQueryValueExW(hKey, nullptr, nullptr, &type, nullptr, &size);
  if (r != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ))
    return false;
  if (size == 0) return false;
  std::vector<wchar_t> buf((size / sizeof(wchar_t)) + 1);
  r = RegQueryValueExW(hKey, nullptr, nullptr, &type,
                       reinterpret_cast<LPBYTE>(buf.data()), &size);
  if (r != ERROR_SUCCESS) return false;
  out.assign(buf.data());
  return true;
}

// Scans HKEY_CLASSES_ROOT\CLSID for a server path that contains 'needle' (e.g.
// "ophir") On success returns true and fills outClsid with the discovered
// CLSID.
static bool findClsidByServerPathSubstring(const std::wstring& needle,
                                           CLSID& outClsid) {
  HKEY hClsid = nullptr;
  if (RegOpenKeyExW(HKEY_CLASSES_ROOT, L"CLSID", 0, KEY_READ, &hClsid) !=
      ERROR_SUCCESS) {
    return false;
  }

  DWORD index = 0;
  const DWORD nameMax = 64;
  std::vector<wchar_t> nameBuf(nameMax);
  DWORD nameSize = nameMax;

  while (true) {
    nameSize = static_cast<DWORD>(nameBuf.size());
    FILETIME ft;
    LONG r = RegEnumKeyExW(hClsid, index++, nameBuf.data(), &nameSize, nullptr,
                           nullptr, nullptr, &ft);
    if (r == ERROR_NO_MORE_ITEMS) break;
    if (r != ERROR_SUCCESS) continue;

    std::wstring subKeyName(nameBuf.data(), nameSize);
    HKEY hSub = nullptr;
    if (RegOpenKeyExW(hClsid, subKeyName.c_str(), 0, KEY_READ, &hSub) !=
        ERROR_SUCCESS)
      continue;

    // Check LocalServer32
    HKEY hLocal = nullptr;
    bool matched = false;
    if (RegOpenKeyExW(hSub, L"LocalServer32", 0, KEY_READ, &hLocal) ==
        ERROR_SUCCESS) {
      std::wstring val;
      if (queryDefaultStringValue(hLocal, val) &&
          containsCaseInsensitive(val, needle)) {
        matched = true;
      }
      RegCloseKey(hLocal);
    }

    // If not matched, check InprocServer32
    if (!matched) {
      HKEY hInproc = nullptr;
      if (RegOpenKeyExW(hSub, L"InprocServer32", 0, KEY_READ, &hInproc) ==
          ERROR_SUCCESS) {
        std::wstring val;
        if (queryDefaultStringValue(hInproc, val) &&
            containsCaseInsensitive(val, needle)) {
          matched = true;
        }
        RegCloseKey(hInproc);
      }
    }

    RegCloseKey(hSub);

    if (matched) {
      // subKeyName should be a GUID string like "{...}, also setting a fallback
      // for later"
      HRESULT hr = CLSIDFromString(subKeyName.c_str(), &outClsid);
      RegCloseKey(hClsid);
      return SUCCEEDED(hr);
    }
  }

  RegCloseKey(hClsid);
  return false;
}

// Discover CLSID using env override -> progid -> registry scan -> fallback
static bool discoverOphirClsid(CLSID& out) {
  static bool attempted = false;
  static CLSID cached{};
  if (attempted) {
    if (cached.Data1 != 0 || cached.Data2 != 0 || cached.Data3 != 0 ||
        cached.Data4[0] != 0) {
      out = cached;
      return true;
    }
    return false;
  }
  attempted = true;

  // 1) env override
  wchar_t* env = _wgetenv(L"OPHIR_CLSID");
  if (env && env[0] != L'\0') {
    CLSID tmp;
    if (SUCCEEDED(CLSIDFromString(env, &tmp))) {
      cached = tmp;
      out = tmp;
      qDebug() << "Ophir CLSID discovered from OPHIR_CLSID env";
      return true;
    }
  }

  // 2) try common ProgIDs (vendor docs may provide a real ProgID)
  const std::vector<const wchar_t*> progids = {
      L"Ophir.Ophir", L"Ophir.PowerMeter", L"Ophir.OphirServer",
      L"OphirCOM.Server"};
  for (auto pid : progids) {
    CLSID tmp;
    if (SUCCEEDED(CLSIDFromProgID(pid, &tmp))) {
      cached = tmp;
      out = tmp;
      qDebug() << "Ophir CLSID discovered from ProgID:"
               << QString::fromWCharArray(pid);
      return true;
    }
  }

  // 3) registry scan for 'ophir' in server path
  CLSID tmp;
  if (findClsidByServerPathSubstring(L"ophir", tmp)) {
    cached = tmp;
    out = tmp;
    qDebug() << "Ophir CLSID discovered by registry scan (found 'ophir' in "
                "server path)";
    return true;
  }

  // 4) fallback to previously known GUID (use as last resort)
  const wchar_t* fallback = L"{b180613c-e514-4739-aadc-cad4493910d7}";
  if (SUCCEEDED(CLSIDFromString(fallback, &tmp))) {
    cached = tmp;
    out = tmp;
    qDebug() << "Ophir CLSID fallback used (hardcoded)";
    return true;
  }

  return false;
}

// New helpers for SetRange / GetWavelengths / SetWavelength / ModifyWavelength
// / SaveSettings
static bool invokeSetRange(IDispatch* d, long h, long ch, long index) {
  DISPID id;
  if (!getDispId(d, L"SetRange", id)) return false;

  VARIANTARG args[3];
  for (int i = 0; i < 3; ++i) VariantInit(&args[i]);

  // DISPPARAMS expects arguments in reverse order (last param first)
  args[0].vt = VT_I4;
  args[0].lVal = index;  // index
  args[1].vt = VT_I4;
  args[1].lVal = ch;  // channel
  args[2].vt = VT_I4;
  args[2].lVal = h;  // handle

  DISPPARAMS dp{args, nullptr, 3, 0};
  return SUCCEEDED(d->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD,
                             &dp, nullptr, nullptr, nullptr));
}

static bool invokeSetWavelength(IDispatch* d, long h, long ch, long index) {
  DISPID id;
  if (!getDispId(d, L"SetWavelength", id)) return false;

  VARIANTARG args[3];
  for (int i = 0; i < 3; ++i) VariantInit(&args[i]);

  // reverse order: index, channel, handle
  args[0].vt = VT_I4;
  args[0].lVal = index;
  args[1].vt = VT_I4;
  args[1].lVal = ch;
  args[2].vt = VT_I4;
  args[2].lVal = h;

  DISPPARAMS dp{args, nullptr, 3, 0};
  return SUCCEEDED(d->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD,
                             &dp, nullptr, nullptr, nullptr));
}

// GetWavelengths(long hDevice, long channel, long& index,
// std::vector<std::string>& options)
static bool invokeGetWavelengths(IDispatch* d, long h, long ch, long& outIndex,
                                 std::vector<QString>& options) {
  DISPID id;
  if (!getDispId(d, L"GetWavelengths", id)) return false;

  VARIANT vOptions;
  VariantInit(&vOptions);

  VARIANT vIndex;
  VariantInit(&vIndex);
  vIndex.vt = VT_I4;
  vIndex.lVal = 0;

  VARIANTARG args[4];
  for (int i = 0; i < 4; ++i) VariantInit(&args[i]);

  // args order (reverse): options (byref variant), index (byref i4), channel,
  // handle
  args[0].vt = VT_VARIANT | VT_BYREF;
  args[0].pvarVal = &vOptions;

  args[1].vt = VT_I4 | VT_BYREF;
  args[1].plVal = &vIndex.lVal;

  args[2].vt = VT_I4;
  args[2].lVal = ch;

  args[3].vt = VT_I4;
  args[3].lVal = h;

  DISPPARAMS dp{args, nullptr, 4, 0};
  HRESULT hr = d->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD,
                         &dp, nullptr, nullptr, nullptr);
  if (FAILED(hr)) {
    VariantClear(&vOptions);
    VariantClear(&vIndex);
    return false;
  }

  options = bstrArrayToQStrings(vOptions);
  outIndex = vIndex.lVal;

  VariantClear(&vOptions);
  VariantClear(&vIndex);
  return true;
}

// ModifyWavelength(long hDevice, long channel, long index, long wavelength);
static bool invokeModifyWavelength(IDispatch* d, long h, long ch, long index,
                                   long wavelength) {
  DISPID id;
  if (!getDispId(d, L"ModifyWavelength", id)) return false;

  VARIANTARG args[4];
  for (int i = 0; i < 4; ++i) VariantInit(&args[i]);

  // reverse order: wavelength, index, channel, handle
  args[0].vt = VT_I4;
  args[0].lVal = wavelength;
  args[1].vt = VT_I4;
  args[1].lVal = index;
  args[2].vt = VT_I4;
  args[2].lVal = ch;
  args[3].vt = VT_I4;
  args[3].lVal = h;

  DISPPARAMS dp{args, nullptr, 4, 0};
  return SUCCEEDED(d->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD,
                             &dp, nullptr, nullptr, nullptr));
}

// SaveSettings(long hDevice, long channel);
static bool invokeSaveSettings(IDispatch* d, long h, long ch) {
  DISPID id;
  if (!getDispId(d, L"SaveSettings", id)) return false;

  VARIANTARG args[2];
  for (int i = 0; i < 2; ++i) VariantInit(&args[i]);

  // reverse order: channel, handle
  args[0].vt = VT_I4;
  args[0].lVal = ch;
  args[1].vt = VT_I4;
  args[1].lVal = h;

  DISPPARAMS dp{args, nullptr, 2, 0};
  return SUCCEEDED(d->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD,
                             &dp, nullptr, nullptr, nullptr));
}

#endif  // _WIN32
}  // namespace

OphirMeterService::OphirMeterService()
    : lastReading_(0.0),
      sensorArea_(0.0),  // Will be set to 5mm circular area
      connected_(false),
      streaming_(false),
      deviceInfo_("Ophir Power Meter"),
      deviceHandle_(-1) {
  // 5mm diameter = 2.5mm radius = 0.25cm radius
  // Area = π * r² = π * (0.25)² ≈ 0.196 cm²
  sensorArea_ = 3.14159265359 * 0.25 * 0.25;  // ~0.196 cm²
}

OphirMeterService::~OphirMeterService() {
  stopStreaming();
  disconnect();
}

// Helper: extract first integer token from preset string (returns success via
// ok)
static int extractFirstIntFromPreset(const QString& s, bool& ok) {
  ok = false;
  QString num;
  bool inNum = false;
  for (QChar c : s) {
    if (c.isDigit()) {
      num.append(c);
      inNum = true;
    } else if (inNum) {
      break;  // stop at end of first number
    }
  }
  if (num.isEmpty()) return 0;
  bool convOk = false;
  int v = num.toInt(&convOk);
  if (convOk) {
    ok = true;
    return v;
  }
  return 0;
}

bool OphirMeterService::connect() {
  try {
    disconnect();

#ifdef _WIN32
    static CoInitGuard co;
    if (FAILED(co.hr) && co.hr != S_FALSE) {
      lastError_ = "COM initialization failed";
      return false;
    }

    CLSID clsid{};
    bool haveClsid = discoverOphirClsid(clsid);
    if (!haveClsid) {
      lastError_ = "Could not discover Ophir CLSID";
      return false;
    }

    HRESULT hr = CoCreateInstance(
        clsid, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
        IID_IDispatch, reinterpret_cast<void**>(&disp_));
    if (FAILED(hr) || !disp_) {
      lastError_ = "CoCreateInstance failed: " + hrHex(hr);
      return false;
    }

    invokeVoid0(disp_, L"StopAllStreams");
    invokeVoid0(disp_, L"CloseAll");

    VARIANT vSerials;
    if (!invokeScanUSB(disp_, vSerials)) {
      lastError_ = "ScanUSB invoke failed";
      return false;
    }

    auto serials = bstrArrayToQStrings(vSerials);
    VariantClear(&vSerials);

    if (serials.empty()) {
      lastError_ = "No Ophir USB devices found";
      return false;
    }

    deviceSerial_ = serials.front();

    long h = 0;
    if (!invokeOpenUSBDevice(disp_, deviceSerial_, h) || h <= 0) {
      lastError_ = "OpenUSBDevice failed";
      return false;
    }
    deviceHandle_ = h;

    bool exists = false;
    if (!invokeIsSensorExists(disp_, deviceHandle_, 0, exists) || !exists) {
      lastError_ = "No sensor connected";
      return false;
    }

    // Retrieve configured sensor area (or set default)
    retrieveSensorArea();

    // Set sensor range to AUTO (index 0 per device docs). Non-fatal if fails.
    if (!invokeSetRange(disp_, deviceHandle_, 0, 0)) {
      qDebug() << "SetRange(AUTO) failed";
    } else {
      qDebug() << "SetRange set to AUTO";
    }

    // Try to set wavelength to 365 nm using presets or by modifying preset
    // index 1 to 365.
    long curIdx = -1;
    std::vector<QString> wlOptions;
    bool gotWl =
        invokeGetWavelengths(disp_, deviceHandle_, 0, curIdx, wlOptions);
    if (gotWl) {
      // search for exact numeric 365 in presets
      int foundIdx = -1;
      for (size_t i = 0; i < wlOptions.size(); ++i) {
        bool ok = false;
        int val = extractFirstIntFromPreset(wlOptions[i], ok);
        if (ok && val == 365) {
          foundIdx = static_cast<int>(i);
          break;
        }
      }

      if (foundIdx >= 0) {
        if (invokeSetWavelength(disp_, deviceHandle_, 0, foundIdx)) {
          qDebug() << "SetWavelength: selected existing preset index"
                   << foundIdx << "for 365 nm";
        } else {
          qDebug() << "SetWavelength failed for existing preset index"
                   << foundIdx;
        }
      } else {
        // Not found: FORCE modify preset index 1 to value 365, save settings,
        // then select it.
        const long targetPresetIndex = 1;
        const long targetWavelength = 365;
        qDebug() << "365 nm preset not found; modifying preset"
                 << targetPresetIndex << "to 365 and saving.";

        bool modOk = invokeModifyWavelength(
            disp_, deviceHandle_, 0, targetPresetIndex, targetWavelength);
        if (!modOk) {
          qDebug() << "ModifyWavelength failed for index" << targetPresetIndex;
        } else {
          qDebug() << "ModifyWavelength succeeded for index"
                   << targetPresetIndex << "->" << targetWavelength;
          bool saveOk = invokeSaveSettings(disp_, deviceHandle_, 0);
          if (!saveOk) {
            qDebug() << "SaveSettings failed after ModifyWavelength";
          } else {
            qDebug() << "SaveSettings succeeded";
          }
          // Now select the preset index 1
          if (invokeSetWavelength(disp_, deviceHandle_, 0, targetPresetIndex)) {
            qDebug() << "SetWavelength: selected preset index"
                     << targetPresetIndex << "after modification";
          } else {
            qDebug() << "SetWavelength failed for preset index"
                     << targetPresetIndex << "after modification";
          }
        }
      }
    } else {
      qDebug()
          << "GetWavelengths invoke failed; cannot inspect or modify presets";
      // as fallback, try to force modify preset1 anyway
      const long targetPresetIndex = 1;
      const long targetWavelength = 365;
      if (invokeModifyWavelength(disp_, deviceHandle_, 0, targetPresetIndex,
                                 targetWavelength)) {
        qDebug() << "ModifyWavelength succeeded (fallback) for index"
                 << targetPresetIndex;
        if (invokeSaveSettings(disp_, deviceHandle_, 0)) {
          qDebug() << "SaveSettings succeeded (fallback)";
          if (invokeSetWavelength(disp_, deviceHandle_, 0, targetPresetIndex)) {
            qDebug() << "SetWavelength selected index" << targetPresetIndex
                     << "after fallback modification";
          } else {
            qDebug() << "SetWavelength failed (fallback) for index"
                     << targetPresetIndex;
          }
        } else {
          qDebug() << "SaveSettings failed (fallback)";
        }
      } else {
        qDebug() << "ModifyWavelength failed (fallback)";
      }
    }

    connected_ = true;
    lastError_.clear();
    deviceInfo_ = ("Ophir Serial: " + deviceSerial_).toStdString();
    qDebug() << "Ophir connected. Handle:" << deviceHandle_;
    return true;
#else
    lastError_ = "Windows only";
    return false;
#endif
  } catch (...) {
    lastError_ = "Unknown connect error";
    return false;
  }
}

void OphirMeterService::disconnect() {
  stopStreaming();
#ifdef _WIN32
  if (disp_) {
    invokeVoid0(disp_, L"StopAllStreams");
    invokeVoid0(disp_, L"CloseAll");
    disp_->Release();
    disp_ = nullptr;
  }
#endif
  deviceHandle_ = -1;
  deviceSerial_.clear();
  connected_ = false;
  qDebug() << "Ophir disconnected";
}

bool OphirMeterService::isConnected() const { return connected_; }

bool OphirMeterService::retrieveSensorArea() {
  // 5mm diameter circular sensor
  // Radius = 2.5mm = 0.25cm
  // Area = π * r² = π * (0.25)² ≈ 0.196 cm²
  const double radius_cm = 0.25;  // 2.5mm = 0.25cm
  sensorArea_ = 3.14159265359 * radius_cm * radius_cm;
  return true;
}

double OphirMeterService::convertPowerToIrradiance(double power_mw) {
  if (sensorArea_ <= 0.0) {
    const double radius_cm = 0.25;  // 2.5mm = 0.25cm (5mm diameter)
    sensorArea_ = 3.14159265359 * radius_cm * radius_cm;
  }
  // Irradiance = Power / Area
  return power_mw / sensorArea_;
}

double OphirMeterService::readIrradiance() {
  if (!connected_ || deviceHandle_ <= 0)
    throw std::runtime_error("Ophir not connected");

#ifdef _WIN32
  invokeStartStop(disp_, L"StartStream", deviceHandle_, 0);

  VARIANT vVals, vTs, vSt;
  if (!invokeGetData(disp_, deviceHandle_, 0, vVals, vTs, vSt)) {
    throw std::runtime_error("GetData failed");
  }

  auto values = r8ArrayToDoubles(vVals);
  VariantClear(&vVals);
  VariantClear(&vTs);
  VariantClear(&vSt);

  if (values.empty()) throw std::runtime_error("No data available");

  const double power_mw = values.back();
  // power_mw returned by device is in mW already; convert to irradiance using
  // sensor area.
  const double irradiance = convertPowerToIrradiance(power_mw * 1000);

  {
    std::lock_guard<std::mutex> lk(readingMutex_);
    lastReading_ = irradiance;
  }

  emit readingChanged(irradiance);
  return irradiance;
#else
  throw std::runtime_error("Unsupported platform");
#endif
}

void OphirMeterService::startStreaming(ReadingCallback callback,
                                       int interval_ms) {
  if (streaming_) return;
  if (!connected_) throw std::runtime_error("Ophir not connected");

#ifdef _WIN32
  invokeStartStop(disp_, L"StartStream", deviceHandle_, 0);
#endif

  streaming_ = true;
  streamThread_ = std::thread(&OphirMeterService::streamingThread, this,
                              callback, interval_ms);
}

void OphirMeterService::stopStreaming() {
  if (!streaming_) return;
  streaming_ = false;
  if (streamThread_.joinable()) streamThread_.join();

#ifdef _WIN32
  if (disp_ && deviceHandle_ > 0)
    invokeStartStop(disp_, L"StopStream", deviceHandle_, 0);
#endif
}

bool OphirMeterService::isStreaming() const { return streaming_; }

double OphirMeterService::getLastReading() const {
  std::lock_guard<std::mutex> lk(readingMutex_);
  return lastReading_;
}

std::string OphirMeterService::getDeviceInfo() const {
  return connected_ ? deviceInfo_ : "Not connected";
}

std::string OphirMeterService::getLastError() const {
  std::lock_guard<std::mutex> lk(errorMutex_);
  return lastError_;
}

void OphirMeterService::streamingThread(ReadingCallback callback,
                                        int interval_ms) {
  while (streaming_) {
    try {
      double v = readIrradiance();
      // qDebug() << "[Ophir] Irradiance mW/cm^2 =" << v;
      callback(v);
    } catch (const std::exception& e) {
      qDebug() << "[Ophir] read error:" << e.what();
    } catch (...) {
      qDebug() << "[Ophir] read error: unknown";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
  }
}