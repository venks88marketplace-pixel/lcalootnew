#include "UVCalibrationProcessor.h"
#include <QTimer>
#include <QThread>
#include <QDebug>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <cmath>
#include <vector>
#include "neo-calibration-server-api/calibration_server_api.pb.h"

UVCalibrationProcessor::UVCalibrationProcessor(const QString& name, 
                                               std::shared_ptr<LoggerService> logger, 
                                               std::shared_ptr<INetworkService> network,
                                               std::shared_ptr<IUVSettingsService> uvSettings,
                                               QObject* parent)
    : QObject(parent),
      name_(name),
      logger_(logger),
      networkService_(network),
      uvSettingsService_(uvSettings),
      connected_(false),
      currentReading_(0.0),
      isCalibrating_(false),
      calibrationProgress_(0),
      shouldStopCalibration_(false),
      ophirMeter_(std::make_unique<OphirMeterService>()),
      calibrationMode_(CalibrationMode::REAL)
{
}

UVCalibrationProcessor::~UVCalibrationProcessor() {
    stopCalibration();
    if (ophirMeter_ && ophirMeter_->isConnected()) {
        ophirMeter_->disconnect();
    }
}

void UVCalibrationProcessor::connectToMeter() {
    if (!ophirMeter_) {
        emit calibrationError("Ophir Meter service not initialized");
        return;
    }

    if (ophirMeter_->connect()) {
        connected_ = true;
        statusMessage_ = "Connected to Ophir Meter";
        
        // Set sensor to 5mm circular area
        ophirMeter_->setSensorArea(0.1963);  // ~0.1963 cm² for 5mm diameter
        
        auto callback = [this](double irradiance) {
            QMetaObject::invokeMethod(this, [this, irradiance]() {
                currentReading_ = irradiance;
                emit readingUpdated();
            });
        };
        
        try {
            ophirMeter_->startStreaming(callback, 100);
            doLogMessage("Started streaming from Ophir Meter (5mm sensor area: 0.1963 cm²)");
        } catch (const std::exception& e) {
            doLogMessage("Failed to start streaming: " + QString::fromStdString(e.what()));
        }
    } else {
        connected_ = false;
        statusMessage_ = "Failed to connect: " + QString::fromStdString(ophirMeter_->getLastError());
        doLogMessage(statusMessage_);
    }
    
    emit connectedChanged(connected_);
    emit statusChanged();
}

void UVCalibrationProcessor::disconnectFromMeter() {
    stopCalibration();
    if (ophirMeter_) {
        ophirMeter_->stopStreaming();
        ophirMeter_->disconnect();
    }
    connected_ = false;
    currentReading_ = 0.0;
    statusMessage_ = "Disconnected from Ophir Meter";
    
    doLogMessage(statusMessage_);
    emit connectedChanged(false);
    emit statusChanged();
    emit readingUpdated();
}

void UVCalibrationProcessor::startCalibration(int mode) {
    if (!connected_) {
        emit calibrationError("Ophir Meter not connected");
        return;
    }
    
    if (isCalibrating_) {
        return;
    }
    
    if (!uvSettingsService_) {
        emit calibrationError("UV Settings Service not available");
        return;
    }
    
    isCalibrating_ = true;
    shouldStopCalibration_ = false;
    calibrationProgress_ = 0;
    calibrationResults_.clear();
    calibrationMode_ = (mode == 0) ? CalibrationMode::REAL : CalibrationMode::DEMO;
    
    doLogMessage("=== Starting UV Calibration ===");
    
    // Start calibration thread with chosen mode
    calibrationThread_ = std::thread([this]() {
        if (calibrationMode_ == CalibrationMode::DEMO) {
            runCalibrationLoopDemo();
        } else {
            runCalibrationLoopReal();
        }
    });
    
    emit calibratingChanged();
    emit progressUpdated();
}

void UVCalibrationProcessor::stopCalibration() {
    if (isCalibrating_) {
        shouldStopCalibration_ = true;
        if (calibrationThread_.joinable()) {
            calibrationThread_.join();
        }
        
        // Turn off UV
        if (uvSettingsService_) {
            doLogMessage("Turning off UV...");
            uvSettingsService_->setEnabled(false);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        isCalibrating_ = false;
        emit calibratingChanged();
    }
}

bool UVCalibrationProcessor::setDacAndWait(uint32_t dac, int waitMs) {
    if (!uvSettingsService_) {
        return false;
    }
    
    uvSettingsService_->setPower(static_cast<int>(dac));
    uvSettingsService_->apply();
    std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
    
    return true;
}

bool UVCalibrationProcessor::sendIrradianceDacPair(double irradiance, uint32_t dac)
{
    if (!networkService_ || !networkService_->isConnected()) {
        doLogMessage("Network not connected, skipping server upload");
        return false;
    }

    try {
        CalibrationApi::ToServer toServer;
        auto* msg = toServer.mutable_start_calibration();

        // Preserve original simplified behavior: cast irradiance to integer (as in your earlier code)
        msg->set_irradiance_mw_per_cm2(static_cast<uint32_t>(irradiance));
        msg->set_dac_count(dac);

        QByteArray data(static_cast<int>(toServer.ByteSizeLong()), '\0');
        if (!toServer.SerializeToArray(data.data(), data.size())) {
            doLogMessage("ERROR: Failed to serialize calibration pair");
            return false;
        }

        networkService_->sendMessage(data);

        std::stringstream ss;
        ss << "  ✓ Sent to server: Irradiance=" << std::fixed << std::setprecision(3)
           << irradiance << " mW/cm² | DAC=" << dac;
        doLogMessage(QString::fromStdString(ss.str()));

        // Keep the 3 second wait used in your simplified test
        std::this_thread::sleep_for(std::chrono::seconds(3));

        return true;
    } catch (const std::exception& e) {
        doLogMessage("ERROR: Sending to server: " + QString::fromStdString(e.what()));
        return false;
    }
}

// Generate an arithmetic sequence of unsigned ints
std::vector<uint32_t> UVCalibrationProcessor::generateSequence(uint32_t start, uint32_t step, size_t count) {
    std::vector<uint32_t> seq;
    if (count == 0 || step == 0) {
        return seq;
    }
    seq.reserve(count);
    uint32_t value = start;
    for (size_t i = 0; i < count; ++i) {
        seq.push_back(value);
        value += step;
    }
    return seq;
}

double UVCalibrationProcessor::readAveragedIrradiance(int numSamples) {
    double sum = 0.0;
    int successfulReads = 0;
    
    for (int i = 0; i < numSamples; ++i) {
        try {
            double reading = ophirMeter_->readIrradiance();
            sum += reading;
            successfulReads++;
        } catch (const std::exception& e) {
            // Silently ignore read errors
        }
        
        if (i < numSamples - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    if (successfulReads == 0) {
        return -1.0;
    }
    
    return sum / successfulReads;
}


void UVCalibrationProcessor::runCalibrationLoopReal() {
    doLogMessage("\n=== REAL MODE CALIBRATION ===");

    // Target irradiance values to calibrate: 3 to 50 mW/cm²
    std::vector<uint32_t> targetIrradianceValues;
    for (uint32_t i = 3; i <= 50; ++i) {
        targetIrradianceValues.push_back(i);
    }

    int totalSteps = static_cast<int>(targetIrradianceValues.size());
    int currentStep = 0;

    // Use centralized header constants
    const uint32_t coarseDacStep = DAC_COARSE_STEP;
    const uint32_t fineDacStep = DAC_FINE_STEP;
    const double tolerancePercent = TOLERANCE_PERCENT;
    const int stabilizationMs = static_cast<int>(STABILIZATION_TIME_MS);
    const int samplesPerReading = SAMPLES_TO_AVERAGE;
    const int maxAttempts = static_cast<int>(MAX_COARSE_ATTEMPTS + MAX_FINE_ATTEMPTS);

    uint32_t lastSuccessfulDac = coarseDacStep;
    double lastSuccessfulMeasuredIrradiance = -1.0;

    for (uint32_t targetIrradiance : targetIrradianceValues) {
        if (shouldStopCalibration_) {
            doLogMessage("Calibration stopped by user");
            break;
        }

        currentStep++;
        calibrationProgress_ = (currentStep * 100) / totalSteps;
        QMetaObject::invokeMethod(this, [this]() { emit progressUpdated(); });

        std::stringstream headerSs;
        headerSs << "\n=== Step " << currentStep << "/" << totalSteps
                 << ": Target Irradiance=" << targetIrradiance << " mW/cm² ===";
        doLogMessage(QString::fromStdString(headerSs.str()));

        // Calculate tolerance bounds
        double lowerBound = static_cast<double>(targetIrradiance) * (1.0 - tolerancePercent / 100.0);
        double upperBound = static_cast<double>(targetIrradiance) * (1.0 + tolerancePercent / 100.0);

        std::stringstream boundsss;
        boundsss << "Tolerance bounds: " << std::fixed << std::setprecision(2)
                 << lowerBound << " - " << upperBound << " mW/cm²";
        doLogMessage(QString::fromStdString(boundsss.str()));

        // Initialize search using last successful DAC as base
        uint32_t currentDac = lastSuccessfulDac;
        bool foundTarget = false;
        int attempt = 0;
        int readRetriesRemaining = READ_RETRY_COUNT;

        // Variables to hold previous attempt's measurement for slope estimation
        double prevMeasured = -1.0;
        uint32_t prevDac = 0;

        std::stringstream startSs;
        startSs << "Starting DAC search from last successful value: " << lastSuccessfulDac;
        doLogMessage(QString::fromStdString(startSs.str()));

        while (!shouldStopCalibration_ && !foundTarget && attempt < maxAttempts) {
            attempt++;

            std::stringstream attemptSs;
            attemptSs << "\nAttempt " << attempt << ": Setting DAC=" << currentDac;
            doLogMessage(QString::fromStdString(attemptSs.str()));

            // Turn ON UV with current DAC
            doLogMessage("Turning ON UV...");
            uvSettingsService_->setPower(static_cast<int>(currentDac));
            uvSettingsService_->setEnabled(true);
            uvSettingsService_->apply();

            // Wait for stabilization
            doLogMessage("Waiting " + QString::number(stabilizationMs / 1000) + " seconds for stabilization...");
            std::this_thread::sleep_for(std::chrono::milliseconds(stabilizationMs));

            // Read irradiance from Ophir meter
            doLogMessage("Reading irradiance from Ophir meter...");
            double measuredIrradiance = readAveragedIrradiance(samplesPerReading);

            // Turn OFF UV
            doLogMessage("Turning OFF UV");
            uvSettingsService_->setEnabled(false);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

            if (measuredIrradiance < 0) {
                // Read failed. Retry same DAC a couple of times before changing it.
                doLogMessage("ERROR: Failed to read irradiance from Ophir meter");
                if (readRetriesRemaining > 0) {
                    readRetriesRemaining--;
                    doLogMessage(QString("Retrying read at same DAC (retries left: %1)").arg(readRetriesRemaining));
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    // decrement attempt to give another try without counting this value as a DAC change
                    continue;
                } else {
                    readRetriesRemaining = READ_RETRY_COUNT;
                    // Treat as transient and step DAC upward (minor upwards move)
                    currentDac = std::min<uint32_t>(currentDac + coarseDacStep, MAX_DAC);
                    // update prevMeasured/prevDac only if we had a prev measurement
                    if (prevMeasured > 0) {
                        prevDac = currentDac;
                    }
                    continue;
                }
            }

            // Reset read retry counter after a successful read
            readRetriesRemaining = READ_RETRY_COUNT;

            // Log the reading
            std::stringstream readSs;
            readSs << "Measured irradiance: " << std::fixed << std::setprecision(3)
                   << measuredIrradiance << " mW/cm²";
            doLogMessage(QString::fromStdString(readSs.str()));

            // Estimate local slope using previous attempt if available
            bool slopeOk = false;
            double slope = 0.0; // mW/cm² per DAC
            if (prevMeasured > 0.0 && prevDac != currentDac) {
                slope = (measuredIrradiance - prevMeasured) / static_cast<double>(static_cast<int>(currentDac) - static_cast<int>(prevDac));
                if (slope > 1e-7) { // requires a reasonable positive slope, could possibly improve on this.
                    slopeOk = true;
                }
            } else if (lastSuccessfulMeasuredIrradiance > 0.0 && lastSuccessfulDac != currentDac) {
                // fallback to slope from last successful point and current measurement if prev unavailable
                slope = (measuredIrradiance - lastSuccessfulMeasuredIrradiance) / static_cast<double>(static_cast<int>(currentDac) - static_cast<int>(lastSuccessfulDac));
                if (slope > 1e-7) {
                    slopeOk = true;
                }
            }

            // Check if within tolerance
            if (measuredIrradiance >= lowerBound && measuredIrradiance <= upperBound) {
                // SUCCESS: Within tolerance range
                std::stringstream successSs;
                successSs << "✓ SUCCESS: Within tolerance! DAC=" << currentDac
                          << " | Irradiance=" << std::fixed << std::setprecision(3)
                          << measuredIrradiance << " mW/cm²";
                doLogMessage(QString::fromStdString(successSs.str()));

                // Store calibration point
                CalibrationPoint point;
                point.target_irradiance_mw_per_cm2 = targetIrradiance;
                point.dac_count = currentDac;
                point.measured_irradiance = measuredIrradiance;
                point.pd1_count = PD1_FIXED_VALUE;
                point.pd2_count = 1000;
                point.pd3_count = 1000;
                point.current_count = 5000;
                point.success = true;
                calibrationResults_.append(point);

                // Update last successful lookup
                lastSuccessfulDac = currentDac;
                lastSuccessfulMeasuredIrradiance = measuredIrradiance;

                // Notify UI with correct arguments: (irradiance, dac, measured)
                QMetaObject::invokeMethod(this, [this, targetIrradiance, currentDac, measuredIrradiance]() {
                    emit calibrationPointAdded(static_cast<uint32_t>(targetIrradiance), currentDac, measuredIrradiance);
                });

                foundTarget = true;
                break;
            }

            // Not within tolerance: decide how to change DAC using slope if possible
            if (measuredIrradiance > upperBound) {
                // ABOVE TARGET: Too high, reduce DAC
                std::stringstream aboveSs;
                aboveSs << "Above target (exceeds +" << tolerancePercent << "%).";
                doLogMessage(QString::fromStdString(aboveSs.str()));

                if (slopeOk) {
                    // Calculations to find out how much DAC to reduce to reach the target (conservative)
                    double overshoot = measuredIrradiance - static_cast<double>(targetIrradiance);
                    double requiredDacReduction = std::ceil(overshoot / slope);
                    uint32_t reduction = static_cast<uint32_t>(std::max<double>(fineDacStep, std::min<double>(requiredDacReduction * PREDICTIVE_CONSERVATIVE_FACTOR, PREDICTIVE_MAX_JUMP_DAC)));
                    if (currentDac > reduction) currentDac -= reduction;
                    else currentDac = 0;
                    std::stringstream ss2;
                    ss2 << "Reducing DAC by " << reduction << " (predicted) to " << currentDac;
                    doLogMessage(QString::fromStdString(ss2.str()));
                } else {
                    // Fallback: small fine decrement
                    std::stringstream ss2;
                    ss2 << "Reducing DAC by fine step " << fineDacStep;
                    doLogMessage(QString::fromStdString(ss2.str()));
                    if (currentDac > fineDacStep) currentDac -= fineDacStep;
                    else currentDac = 0;
                }
            } else {
                // BELOW TARGET: Too low, increase DAC
                std::stringstream belowSs;
                belowSs << "Below target (below -" << tolerancePercent << "%).";
                doLogMessage(QString::fromStdString(belowSs.str()));

                if (slopeOk) {
                    double deficit = static_cast<double>(targetIrradiance) - measuredIrradiance;
                    double requiredDacIncrease = std::ceil(deficit / slope);
                    // Apply conservative factor and clamp
                    double jumped = std::max<double>(PREDICTIVE_MIN_JUMP_DAC, std::min<double>(requiredDacIncrease * PREDICTIVE_CONSERVATIVE_FACTOR, PREDICTIVE_MAX_JUMP_DAC));
                    uint32_t jump = static_cast<uint32_t>(std::max<double>(static_cast<double>(coarseDacStep), std::floor(jumped)));
                    currentDac = std::min<uint32_t>(currentDac + jump, MAX_DAC);
                    std::stringstream ss2;
                    ss2 << "Increasing DAC by " << jump << " (predicted) to " << currentDac;
                    doLogMessage(QString::fromStdString(ss2.str()));
                } else {
                    // Conservative fallback: small coarse increment
                    std::stringstream ss2;
                    ss2 << "Increasing DAC by coarse step " << coarseDacStep;
                    doLogMessage(QString::fromStdString(ss2.str()));
                    currentDac = std::min<uint32_t>(currentDac + coarseDacStep, MAX_DAC);
                }
            }

            // Save this measurement as previous for next slope estimate
            prevMeasured = measuredIrradiance;
            prevDac = currentDac;
        } // end attempts loop

        if (!foundTarget) {
            // Failed to find target after max attempts
            std::stringstream failSs;
            failSs << "ERROR: Failed to find DAC for target " << targetIrradiance
                   << " mW/cm² after " << maxAttempts << " attempts";
            doLogMessage(QString::fromStdString(failSs.str()));

            CalibrationPoint failPt;
            failPt.target_irradiance_mw_per_cm2 = targetIrradiance;
            failPt.dac_count = currentDac;
            failPt.measured_irradiance = -1.0;
            failPt.pd1_count = PD1_FIXED_VALUE;
            failPt.pd2_count = 1000;
            failPt.pd3_count = 1000;
            failPt.current_count = 5000;
            failPt.success = false;
            failPt.error_message = "Max attempts exceeded";
            calibrationResults_.append(failPt);

            QMetaObject::invokeMethod(this, [this, targetIrradiance]() {
                emit calibrationPointAdded(static_cast<uint32_t>(targetIrradiance), 0, -1.0);
            });
        }

        // Small delay between targets (keep brief)
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    } // end target loop

    // Calibration run complete — now upload results in one shot, works better because I see timeouts on the server side when waiting too long or sending too fast.
    sendBatchedCalibrationResults();

    isCalibrating_ = false;
    calibrationProgress_ = 100;

    std::stringstream finalSs;
    finalSs << "\n=== REAL Calibration Complete ===\n"
            << "Total points collected: " << calibrationResults_.size();
    doLogMessage(QString::fromStdString(finalSs.str()));

    QMetaObject::invokeMethod(this, [this]() {
        emit calibratingChanged();
        emit progressUpdated();
        emit calibrationComplete();
    });
}

// Upload all successful calibration points
void UVCalibrationProcessor::sendBatchedCalibrationResults() {
    if (!networkService_ || !networkService_->isConnected()) {
        doLogMessage("Network not connected, skipping batched upload");
        return;
    }

    try {
        for (const CalibrationPoint& p : calibrationResults_) {
            if (!p.success) continue;

            CalibrationApi::ToServer toServer;
            auto* msg = toServer.mutable_start_calibration();
            msg->set_irradiance_mw_per_cm2(p.target_irradiance_mw_per_cm2);
            msg->set_dac_count(p.dac_count);

            QByteArray data(static_cast<int>(toServer.ByteSizeLong()), '\0');
            if (!toServer.SerializeToArray(data.data(), data.size())) {
                doLogMessage("ERROR: Failed to serialize calibration pair for batched upload");
                continue;
            }

            networkService_->sendMessage(data);

            std::stringstream ss;
            ss << "  ✓ Batched sent to server: Irradiance=" << std::fixed << std::setprecision(3)
               << p.target_irradiance_mw_per_cm2 << " mW/cm² | DAC=" << p.dac_count
               << " | Measured=" << p.measured_irradiance;
            doLogMessage(QString::fromStdString(ss.str()));
            // Safety sleep to avoid the timeout from server side.
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        }
    } catch (const std::exception& e) {
        doLogMessage("ERROR: Batched upload exception: " + QString::fromStdString(e.what()));
    }
}

void UVCalibrationProcessor::runCalibrationLoopDemo() {
    doLogMessage("\n=== TEST: Sending dummy calibration data ===");
    
    // Use header constants for demo range and increment
    std::vector<uint32_t> irradianceValues;
    for (uint32_t i = DEMO_MIN_IRRADIANCE; i <= DEMO_MAX_IRRADIANCE; ++i) {
        irradianceValues.push_back(i);
    }
    
    // DAC values: start at DEMO_DAC_INCREMENT, increment by DEMO_DAC_INCREMENT for each step
    std::vector<uint32_t> dacValues;
    uint32_t dacValue = DEMO_DAC_INCREMENT;
    for (size_t i = 0; i < irradianceValues.size(); ++i) {
        dacValues.push_back(dacValue);
        dacValue += DEMO_DAC_INCREMENT;
    }
    
    int totalSteps = static_cast<int>(irradianceValues.size());
    int currentStep = 0;
    
    for (size_t idx = 0; idx < irradianceValues.size(); ++idx) {
        if (shouldStopCalibration_) {
            doLogMessage("Calibration stopped by user");
            break;
        }
        
        uint32_t irradiance = irradianceValues[idx];
        uint32_t dac = dacValues[idx];
        
        currentStep++;
        calibrationProgress_ = (currentStep * 100) / totalSteps;
        
        QMetaObject::invokeMethod(this, [this]() {
            emit progressUpdated();
        });
        
        std::stringstream headerSs;
        headerSs << "\n--- Step " << currentStep << "/" << totalSteps 
                 << ": Irradiance=" << irradiance << " mW/cm² | DAC=" << dac << " ---";
        doLogMessage(QString::fromStdString(headerSs.str()));
        
        try {
            CalibrationApi::ToServer toServer;
            auto* msg = toServer.mutable_start_calibration();
            msg->set_irradiance_mw_per_cm2(irradiance);
            msg->set_dac_count(dac);

            QByteArray data(static_cast<int>(toServer.ByteSizeLong()), '\0');
            if (!toServer.SerializeToArray(data.data(), data.size())) {
                doLogMessage("ERROR: Failed to serialize");
                isCalibrating_ = false;
                return;
            }

            networkService_->sendMessage(data);
            
            std::stringstream ss;
            ss << "✓ Sent to server: Irradiance=" << irradiance 
               << " mW/cm² | DAC=" << dac;
            doLogMessage(QString::fromStdString(ss.str()));
            
        } catch (const std::exception& e) {
            doLogMessage("ERROR: " + QString::fromStdString(e.what()));
        }
        
        QMetaObject::invokeMethod(this, [this, dac, irradiance]() {
            emit calibrationPointAdded(dac, dac, static_cast<double>(irradiance));
        });
        
        // Small delay between steps
        std::this_thread::sleep_for(std::chrono::milliseconds(4000));
    }
    
    isCalibrating_ = false;
    calibrationProgress_ = 100;
    
    std::stringstream finalSs;
    finalSs << "\n=== Calibration Complete ===\n"
            << "Total points sent: " << irradianceValues.size();
    doLogMessage(QString::fromStdString(finalSs.str()));
    
    QMetaObject::invokeMethod(this, [this]() {
        emit calibratingChanged();
        emit progressUpdated();
        emit calibrationComplete();
    });
}

CalibrationPoint UVCalibrationProcessor::findDacForIrradianceReal(uint32_t targetIrradiance, uint32_t startingDac)
{
    // Simple wrapper that attempts to find DAC by running a small search.
    // The full search logic is implemented in runCalibrationLoopReal; this function provides
    // a stubbed point if you need to call it separately.
    CalibrationPoint result;
    result.target_irradiance_mw_per_cm2 = targetIrradiance;
    result.success = false;
    result.pd1_count = PD1_FIXED_VALUE;
    result.pd2_count = 1000;
    result.pd3_count = 1000;
    result.current_count = 5000;
    return result;
}

void UVCalibrationProcessor::doLogMessage(const QString& message) {
    qDebug() << message;
    emit logMessage(message);
}