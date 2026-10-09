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

                if (score >= 0.363) {
                    ownerDetected = true;
                    cv::rectangle(frame, cv::Rect(x, y, w, h), cv::Scalar(0, 255, 0), 2); // Green
                    cv::putText(frame, "AUTHORIZED", cv::Point(x, y - 10),
                                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
                } else {
                    intruderDetected = true; // <-- Flags any unauthorized face in frame
                    cv::rectangle(frame, cv::Rect(x, y, w, h), cv::Scalar(0, 0, 255), 2); // Red
                    cv::putText(frame, "UNAUTHORIZED", cv::Point(x, y - 10),
                                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
                }
            }

            // -----------------------------------------------------------------
            // Strict Privacy Condition:
            // Safe ONLY IF owner is present AND no intruder is looking over shoulder
            // -----------------------------------------------------------------
            bool isSafe = ownerDetected && !intruderDetected;

            if (isSafe) {
                // Owner is alone at the workstation -> Reset/cancel any active countdown
                if (countdownActive) {
                    countdownActive = false;
                    lastEmittedSecond = -1;
                    emit statusUpdate("System Active - Authorized User Verified");
                }
            } else {
                // Unsafe condition: either an intruder is present OR no user is in front of camera
                if (!countdownActive) {
                    countdownActive = true;
                    graceTimer.start();
                    lastEmittedSecond = GRACE_PERIOD_SECONDS;

                    if (intruderDetected) {
                        emit statusUpdate(QString("ALERT: Unauthorized Person Detected! Locking in %1s...").arg(GRACE_PERIOD_SECONDS));
                    } else {
                        emit statusUpdate(QString("WARNING: No User Detected! Locking in %1s...").arg(GRACE_PERIOD_SECONDS));
                    }
                }

                qint64 elapsedMs = graceTimer.elapsed();
                int remainingSec = GRACE_PERIOD_SECONDS - static_cast<int>(elapsedMs / 1000);

                if (remainingSec > 0) {
                    if (remainingSec != lastEmittedSecond) {
                        lastEmittedSecond = remainingSec;
                        if (intruderDetected) {
                            emit statusUpdate(QString("ALERT: Unauthorized person present! Locking in %1s...").arg(remainingSec));
                        } else {
                            emit statusUpdate(QString("WARNING: Locking workstation in %1s...").arg(remainingSec));
                        }
                    }

                    // Render prominent visual warning banner on top of the webcam feed
                    std::string alertBanner = intruderDetected ? "INTRUDER DETECTED! LOCKING IN " + std::to_string(remainingSec) + "s"
                                                               : "NO USER PRESENT! LOCKING IN " + std::to_string(remainingSec) + "s";

                    cv::putText(frame, alertBanner, cv::Point(20, 50),
                                cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2);
                } else {
                    // Grace period elapsed without clearing the room -> Lock workstation
                    emit statusUpdate("SECURITY BREACH: Locking Workstation NOW!");
                    cv::putText(frame, "LOCKING WORKSTATION...", cv::Point(20, 50),
                                cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 255), 3);

                    QImage qimg(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_BGR888);
                    emit frameReady(qimg.copy());

                    LockWorkStation();

                    countdownActive = false;
                    lastEmittedSecond = -1;

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
