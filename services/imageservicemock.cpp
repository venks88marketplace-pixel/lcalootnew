#include <QtDebug>
#include <QThread>
#include "opencv2/opencv.hpp"
#include <math.h>
#include "imageservicemock.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

//using namespace std;

namespace imaging {
    ImageServiceMock::ImageServiceMock(QVariantMap folders, QObject *parent)
    : IImageService(parent), m_folders{std::move(folders)}
    {
        qDebug() << " ***************************** ImageServiceMock instantiated!";
    }

    void ImageServiceMock::getImagePair(IImageService::Caller caller, int index )
    {
        if(caller != m_currentCaller) {
            //build list of image pairs
            m_currentCaller = caller;
            m_imagesByPlanes = groupMockImagesByPlane();

            qDebug() << " New caller" << caller;
        }
        qDebug() << "requesting images";
        std::pair<std::string, std::string> imagesPath = m_imagesByPlanes.at(index);
        std::string leftPath = imagesPath.first;
        std::string rightPath = imagesPath.second;

        QImage l = imageFileToQmage( leftPath);
        QImage r = imageFileToQmage( rightPath);

        m_leftImage = QPixmap::fromImage(l);
        m_rightImage = QPixmap::fromImage(r);

        qDebug() << "----------------------Emitting imagesReady from:" << QThread::currentThread() <<Qt::flush;
        emit imagesReady(m_leftImage, m_rightImage);
    }

    QImage ImageServiceMock::imageFileToQmage( std::string path ){
        cv::Mat image = cv::imread(path,cv::IMREAD_GRAYSCALE);
        QImage qimage;
        if (!image.empty()) {
            qimage = QImage(image.data, image.cols, image.rows, image.step, QImage::Format_Grayscale8).copy();
        }
        return qimage;
    }

    void ImageServiceMock::updateImages(const QPixmap& l, const QPixmap& r)
    {
        emit imagesReady(l,r);
    }

    //
    std::vector<std::pair<std::string, std::string>> ImageServiceMock::groupMockImagesByPlane(){
        std::vector<std::pair<std::string, std::string>> imagefilenames;
        if (m_folder_map.find(m_currentCaller) == m_folder_map.end())
        {
            qDebug() << "Error: no folder defineed for caller " << m_currentCaller;
            return imagefilenames;
        }
        QString folder = QString::fromStdString(m_folder_map.at(m_currentCaller));
        if (!m_folders.contains(folder))
        {
            qDebug() << "Error: no folder defineed in appconfig for " << folder;
            return imagefilenames;
        }
        fs::path basepath = m_folders[folder].toString().toStdString();
        auto catalogFile = basepath / "ImageList3D.csv";
        std::ifstream file(catalogFile );

        if (!file.is_open()) {
            qDebug() << "Error: Could not open file: " << catalogFile.c_str();
            return imagefilenames;
        }
        std::string line;
        size_t lineCount = 0;

        std::vector<std::string> tokens;
        std::string token;
        while (std::getline(file, line)) {
            ++lineCount;
            if (line.empty()) continue;
            std::istringstream tokenStream(line);
            while (std::getline(tokenStream, token, ',')) {
                tokens.push_back(token);
            }
            fs::path impathL = basepath / (token + "-left.png");
            fs::path impathR = basepath / (token + "-right.png");
            std::pair<std::string, std::string> pair(impathL.string(), impathR.string());
            imagefilenames.push_back(pair);
        }
        file.close();
        qDebug()  << "Total lines read: " << lineCount;
        return imagefilenames;
    }
}
