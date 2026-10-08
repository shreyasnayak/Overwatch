#define NOMINMAX // Prevent Windows API min/max macro definitions

#include "privacythread.h"
#include <QCoreApplication>
#include <QDebug>

// OpenCV Headers (MUST be included before windows.h)
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>

// Windows API
#include <windows.h>

PrivacyThread::PrivacyThread(QObject *parent) : QThread(parent), m_running(false) {}
PrivacyThread::~PrivacyThread() { stop(); wait(); }
void PrivacyThread::stop() { m_running = false; }

void PrivacyThread::run()
{
    m_running = true;

    try {
        emit statusUpdate("Loading OpenCV AI Models...");
        QString appDir = QCoreApplication::applicationDirPath();
        std::string modelsDir = (appDir + "/models/").toStdString();
        std::string facePath = (appDir + "/my_face.jpg").toStdString();

        // 1. Initialize YuNet Detector & SFace Recognizer
        cv::Ptr<cv::FaceDetectorYN> detector = cv::FaceDetectorYN::create(
            modelsDir + "face_detection_yunet_2023mar.onnx", "", cv::Size(320, 320), 0.6f, 0.3f, 5000);

        cv::Ptr<cv::FaceRecognizerSF> recognizer = cv::FaceRecognizerSF::create(modelsDir + "face_recognition_sface_2021dec.onnx", "");

        // 2. Load Reference Face
        cv::Mat refImg = cv::imread(facePath);
        if (refImg.empty()) {
            emit statusUpdate("Error: Could not find my_face.jpg!");
            return;
        }

        detector->setInputSize(refImg.size());
        cv::Mat refFaces;
        detector->detect(refImg, refFaces);

        if (refFaces.rows < 1) {
            emit statusUpdate("Error: No face detected in my_face.jpg!");
            return;
        }

        // Extract reference feature vector
        cv::Mat refAligned, refFeature;
        recognizer->alignCrop(refImg, refFaces.row(0), refAligned);
        recognizer->feature(refAligned, refFeature);

        // 3. Open Camera Feed
        cv::VideoCapture cap(0);
        if (!cap.isOpened()) {
            emit statusUpdate("Error: Unable to open webcam.");
            return;
        }

        detector->setInputSize(cv::Size(640, 480));
        cv::Mat frame;
        emit statusUpdate("System Active - Monitoring");

        while (m_running) {
            cap >> frame;
            if (frame.empty()) continue;

            cv::Mat faces;
            detector->detect(frame, faces);

            bool intruderDetected = false;

            for (int i = 0; i < faces.rows; i++) {
                cv::Mat alignedFace, feature;
                recognizer->alignCrop(frame, faces.row(i), alignedFace);
                recognizer->feature(alignedFace, feature);

                // Compute cosine similarity score (Threshold ~ 0.363)
                double score = recognizer->match(refFeature, feature, cv::FaceRecognizerSF::DisType::FR_COSINE);

                int x = static_cast<int>(faces.at<float>(i, 0));
                int y = static_cast<int>(faces.at<float>(i, 1));
                int w = static_cast<int>(faces.at<float>(i, 2));
                int h = static_cast<int>(faces.at<float>(i, 3));

                if (score < 0.363) {
                    intruderDetected = true;
                    cv::rectangle(frame, cv::Rect(x, y, w, h), cv::Scalar(0, 0, 255), 2); // Red
                } else {
                    cv::rectangle(frame, cv::Rect(x, y, w, h), cv::Scalar(0, 255, 0), 2); // Green
                }
            }

            QImage qimg(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_BGR888);
            emit frameReady(qimg.copy());

            if (intruderDetected) {
                emit statusUpdate("Intruder Detected! Locking workstation...");
                LockWorkStation();
                QThread::sleep(5);
                emit statusUpdate("System Active - Monitoring");
            }
        }
        cap.release();
    } catch (const std::exception& e) {
        emit statusUpdate(QString("OpenCV Exception: ") + e.what());
    }
}
