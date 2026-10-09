#define NOMINMAX // Prevent Windows API min/max macro definitions

#include "privacythread.h"
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>

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

        cv::Ptr<cv::FaceRecognizerSF> recognizer = cv::FaceRecognizerSF::create(
            modelsDir + "face_recognition_sface_2021dec.onnx", "");

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

        // Countdown State Variables
        bool countdownActive = false;
        QElapsedTimer graceTimer;
        int lastEmittedSecond = -1;
        const int GRACE_PERIOD_SECONDS = 5;

        while (m_running) {
            cap >> frame;
            if (frame.empty()) continue;

            cv::Mat faces;
            detector->detect(frame, faces);

            bool ownerDetected = false;

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

                if (score >= 0.363) {
                    ownerDetected = true;
                    cv::rectangle(frame, cv::Rect(x, y, w, h), cv::Scalar(0, 255, 0), 2); // Green
                    cv::putText(frame, "AUTHORIZED", cv::Point(x, y - 10),
                                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
                } else {
                    cv::rectangle(frame, cv::Rect(x, y, w, h), cv::Scalar(0, 0, 255), 2); // Red
                    cv::putText(frame, "UNAUTHORIZED", cv::Point(x, y - 10),
                                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
                }
            }

            // -----------------------------------------------------------------
            // Lock Grace Period & Timer Logic
            // -----------------------------------------------------------------
            if (ownerDetected) {
                // If owner returns during countdown, cancel the lock process
                if (countdownActive) {
                    countdownActive = false;
                    lastEmittedSecond = -1;
                    emit statusUpdate("System Active - Authorized User Verified");
                }
            } else {
                // Owner is NOT detected (either intruder present or user stepped away)
                if (!countdownActive) {
                    countdownActive = true;
                    graceTimer.start();
                    lastEmittedSecond = GRACE_PERIOD_SECONDS;
                    emit statusUpdate(QString("WARNING: Unauthorized/No User! Locking in %1s...").arg(GRACE_PERIOD_SECONDS));
                }

                qint64 elapsedMs = graceTimer.elapsed();
                int remainingSec = GRACE_PERIOD_SECONDS - static_cast<int>(elapsedMs / 1000);

                if (remainingSec > 0) {
                    if (remainingSec != lastEmittedSecond) {
                        lastEmittedSecond = remainingSec;
                        emit statusUpdate(QString("WARNING: Locking workstation in %1 seconds...").arg(remainingSec));
                    }

                    // Render large visual timer banner directly on video frame
                    std::string timerText = "WARNING: LOCKING IN " + std::to_string(remainingSec) + "s";
                    cv::putText(frame, timerText, cv::Point(30, 50),
                                cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 255), 3);
                } else {
                    // 5-second grace period expired without authorized verification -> Lock Workstation
                    emit statusUpdate("INTRUDER CONFIRMED: Locking Workstation NOW!");
                    cv::putText(frame, "LOCKING WORKSTATION...", cv::Point(30, 50),
                                cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 255), 3);

                    // Send final frame with banner before locking
                    QImage qimg(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_BGR888);
                    emit frameReady(qimg.copy());

                    LockWorkStation();

                    countdownActive = false;
                    lastEmittedSecond = -1;

                    // Pause thread briefly so it doesn't re-trigger immediately upon Windows unlock
                    QThread::sleep(5);
                    emit statusUpdate("System Active - Monitoring");
                    continue;
                }
            }

            QImage qimg(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_BGR888);
            emit frameReady(qimg.copy());
        }
        cap.release();
    } catch (const std::exception& e) {
        emit statusUpdate(QString("OpenCV Exception: ") + e.what());
    }
}
