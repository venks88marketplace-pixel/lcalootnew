#pragma once

#include <QObject>
#include <QImage>
#include <QPixmap>
#include <QColor>
#include <QQmlContext>
#include <QTimer>
#include <interfaces/IImageService.h>
#include <interfaces/INetworkService.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace imaging {
    class ImageServiceMock : public IImageService
    {
        Q_OBJECT

    public:
        using IImageService::IImageService;
        ImageServiceMock(QVariantMap folders, QObject *parent);

        explicit ImageServiceMock(const ImageServiceMock& rhs) = delete;
        ImageServiceMock& operator= (const ImageServiceMock& rhs) = delete;

        Q_INVOKABLE void startVideo() override {}
        Q_INVOKABLE void stopVideo() override {};
        void configure(const QJsonObject& settings) override {}

        virtual void updateImages(const QPixmap& l, const QPixmap& r) override;

        std::vector<std::pair<std::string, std::string>>& imagesByPlane () { return m_imagesByPlanes; }

    public slots:
        Q_INVOKABLE void getImagePair(IImageService::Caller caller, int index) override;

    private:
        std::vector<std::pair<std::string, std::string>> groupMockImagesByPlane();
        int m_imageIndex;
        QImage imageFileToQmage( std::string path );

        const std::map<IImageService::Caller, std::string> m_folder_map {
            {IImageService::Caller::StereoCal, "stereoFolder"},
            {IImageService::Caller::StereoVerification, "stereoVerificationFolder"},
            {IImageService::Caller::PhotometricCal, "photometricFolder"},
            {IImageService::Caller::DMDCal, "dmdFolder"},
            {IImageService::Caller::DMDVerification, "dmdCheckFolder"}
        };
        QVariantMap m_folders;
        std::vector<std::pair<std::string, std::string>> m_imagesByPlanes;
        IImageService::Caller m_currentCaller{IImageService::Caller::Unknown};
    };
}

