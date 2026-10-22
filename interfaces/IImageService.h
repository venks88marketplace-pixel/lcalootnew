#pragma once

#include <QObject>
#include <QPixmap>
#include <QImage>
#include <qqmlintegration.h>
#include "IService.h"

namespace imaging {
    class IImageService : public IService
    {
        Q_OBJECT

    public:
        enum class Caller {
            Unknown,
            OffsetCal,
            PhotometricCal,
            StereoCal,
            StereoVerification,
            DMDCal,
            DMDVerification
        };
        Q_ENUM(Caller)

        using IService::IService;
        virtual ~IImageService() override = default;

        virtual  void startVideo() = 0;
        virtual  void stopVideo() = 0;

        virtual void startWideVideo() { }
        virtual void stopWideVideo() { }

        Q_INVOKABLE const QPixmap& leftImage() const { return m_leftImage; }
        Q_INVOKABLE const QPixmap& rightImage() const { return m_rightImage; }

        virtual void updateImages(const QPixmap& l, const QPixmap& r) = 0;

        // Controls whether the raw (un-stretched) WFOV frame is
        // decoded and emitted via wideFrameReady(). Default is a
        // no-op so existing implementations are unaffected unless
        // they opt in (see ImageService override).
        virtual void setWideMeasurementActive(bool active) { Q_UNUSED(active); }
        virtual void setWideExposure(double exposureUs, double gain = 0.0) { Q_UNUSED(exposureUs); Q_UNUSED(gain); }

    signals:
        void imagesReady(QPixmap imageL, QPixmap imageR);
        void wideFrameReady(const QImage& image);

    public slots:
        virtual void getImagePair(Caller caller = Caller::Unknown, int index = 0) = 0;
    protected:
         QPixmap m_leftImage;
         QPixmap m_rightImage;
    };
}