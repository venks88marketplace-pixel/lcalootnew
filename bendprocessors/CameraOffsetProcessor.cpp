#include "CameraOffsetProcessor.h"

using imaging::IImageService;

CameraOffsetProcessor::CameraOffsetProcessor(QString name,
                    std::shared_ptr<ILoggerService> logger,
                    std::shared_ptr<IServerProxyService> proxy,
                    std::shared_ptr<IImageService> imageprovider,
                    std::shared_ptr<IStepperControllerService> stepperService,
                    QObject* parent)
    : BaseProcessor(name, std::move(logger), std::move(proxy), std::move(imageprovider), std::move(stepperService), parent)
{
    m_imageProcessor = std::make_unique<ImageProcessor>(parent);
    qDebug() << "CameraOffsetProcessor CREATED";
}

void CameraOffsetProcessor::execute(const QString& action) {
    IImageService* svc = qobject_cast<IImageService*>(imageService());
    if (!svc ) {
        qWarning() << "svc is not an ImageService";
        return;
    }
    Q_ASSERT_X(!svc->leftImage().isNull() && !svc->rightImage().isNull(),
               "process images", "invalid image(s)");
    try {
        bool  res = m_imageProcessor->computeOffsets( svc->leftImage(), svc->rightImage());
        if (res) {
            leftOverlay()->addCircle(m_imageProcessor->leftImageUVCenter().first,
                 m_imageProcessor->leftImageUVCenter().second, 10, qRgba(0,255,255,255), 1);
            leftOverlay()->addRect(790,894,144,120, qRgba(128,0,255,255));
            leftOverlay()->addRect(m_imageProcessor->searchRoiLeft().x,m_imageProcessor->searchRoiLeft().y,
                 m_imageProcessor->searchRoiLeft().width, m_imageProcessor->searchRoiLeft().height, qRgba(255,0,255,255), 1);

            rightOverlay()->addCircle(m_imageProcessor->rightImageUVCenter().first,
                m_imageProcessor->rightImageUVCenter().second, 10, qRgba(0,255,255,255), 1);
            leftOverlay()->addRect(m_imageProcessor->searchRoiRight().x,m_imageProcessor->searchRoiRight().y,
                m_imageProcessor->searchRoiRight().width, m_imageProcessor->searchRoiRight().height, qRgba(255,0,255,255), 1);
        }
    }catch (const std::exception& e){
        qDebug() << "calling computeOffsets failed";
    }
}


