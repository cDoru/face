#ifndef TESTFACEFEATURESDETECTION_H
#define TESTFACEFEATURESDETECTION_H

#include <QDebug>
#include <QApplication>
#include <opencv/cv.h>
#include <QDir>
#include <QFileInfoList>

#include "facelib/glwidget.h"
#include "facelib/mesh.h"
#include "facelib/map.h"
#include "facelib/surfaceprocessor.h"
#include "facelib/landmarkdetector.h"
#include "linalg/vector.h"
#include "linalg/procrustes.h"
#include "linalg/pointcloud.h"
#include "facelib/maskedvector.h"
#include "facelib/facefeaturesanotation.h"
#include "facelib/landmarks.h"
#include "linalg/pca.h"
#include "facelib/widgetmeshselect.h"

enum AlignType
{
    simple, isoCurve, triangle
};

class TestFaceFeatuesDetection
{
public:
    static int testDepthmapProcessing(int argc, char *argv[], QString pathToOBJ)
    {
        Mesh face = Mesh::fromOBJ(pathToOBJ);
        SurfaceProcessor::smooth(face, 0.5, 5);

        MapConverter converter;
        Map depthMap = SurfaceProcessor::depthmap(face, converter, 1);
        SurfaceProcessor::smooth(depthMap, 0.5, 20);

        Matrix depthImage = depthMap.toMatrix();
        cv::imshow("depthMap", depthImage);

        CurvatureStruct cs = SurfaceProcessor::calculateCurvatures(depthMap);

        Matrix gauss = cs.curvatureGauss.toMatrix();
        cv::imshow("gauss", gauss);

        Matrix mean = cs.curvatureMean.toMatrix();
        cv::imshow("mean", mean);

        Matrix curvatureImage = cs.curvatureIndex.toMatrix();
        cv::imshow("curvatureIndex", curvatureImage);

        Matrix peaks = cs.peaks.toMatrix(0, 0, 1);
        cv::imshow("peaks", peaks);

        Matrix pits = cs.pits.toMatrix(0, 0, 1);
        cv::imshow("pits", pits);

        Matrix peaksDensity = cs.peaks.densityMap(21, true).toMatrix();
        cv::imshow("peaksDensity", peaksDensity);

        QApplication app(argc, argv);
        GLWidget widget;
        widget.setWindowTitle("GL Widget");
        widget.addFace(&face);
        widget.show();

        return app.exec();
    }

    static int  testBatchLandmarkDetection(int argc, char *argv[], QString dirPath)
    {
        /*QApplication app(argc, argv);
        WidgetMeshSelect widget;
        QStringList filter; filter << "*.obj";
        widget.setPath(dirPath, filter);
        widget.show();
        return app.exec();*/

        QDir dir(dirPath);
        QStringList filter; filter << "*.obj";
        QFileInfoList list = dir.entryInfoList(filter, QDir::Files);
        foreach (const QFileInfo &info, list)
        {
            Mesh m = Mesh::fromOBJ(info.absoluteFilePath());
            LandmarkDetector detector(m);
            Landmarks l = detector.Detect();
            QString lPath = dirPath + QDir::separator() + info.baseName() + "_auto.xml";
            l.serialize(lPath);
        }
    }

    static int  testSuccessBatchLandmarkDetection(QString dirPath)
    {
        QDir dir(dirPath);
        QStringList filter; filter << "*.obj";
        QFileInfoList list = dir.entryInfoList(filter, QDir::Files);
        foreach (const QFileInfo &info, list)
        {
            Mesh m = Mesh::fromOBJ(info.absoluteFilePath());
            QString lPath = dirPath + QDir::separator() + info.baseName() + ".xml";
            Landmarks l(lPath);

            MapConverter converter;
            Map depth = SurfaceProcessor::depthmap(m, converter, 1.0);
            Matrix img = depth.toMatrix() * 255;

            for (int i = 0; i < l.points.size(); i++)
            {
                const cv::Point3d &p = l.points[i];
                cv::Point2d mapPoint = converter.MeshToMapCoords(depth, p);
                cv::circle(img, cv::Point(mapPoint.x, mapPoint.y), 2, 0);

                if (i > 0)
                {
                    cv::Point2d prevPoint = converter.MeshToMapCoords(depth, l.points[i-1]);
                    cv::line(img, prevPoint, mapPoint, 0);
                }
            }

            QString imgPath = dirPath + QDir::separator() + info.baseName() + ".png";
            cv::imwrite(imgPath.toStdString(), img);
        }
    }

    static void exportInitialEstimationsForVOSM(QString dirPath)
    {
        QDir dir(dirPath);
        QStringList filter; filter << "*.obj";
        QFileInfoList list = dir.entryInfoList(filter, QDir::Files);
        foreach (const QFileInfo &info, list)
        {
            Mesh m = Mesh::fromOBJ(info.absoluteFilePath());
            QString lPath = dirPath + QDir::separator() + info.baseName() + "_auto.xml";
            Landmarks l(lPath);

            MapConverter converter;
            Map depthmap = SurfaceProcessor::depthmap(m, converter, 1.0);

            QString ptsPath = dirPath + QDir::separator() + info.baseName() + ".pts";
            QFile ptsFile(ptsPath);
            ptsFile.open(QFile::WriteOnly);
            QTextStream ptsStream(&ptsFile);
            ptsStream << "version: 1\nn_points: 3\n{\n";

            cv::Point2d p2d;
            p2d = converter.MeshToMapCoords(depthmap, l.points[Landmarks::LeftInnerEye]);
            ptsStream << p2d.x << " " << p2d.y << "\n";
            p2d = converter.MeshToMapCoords(depthmap, l.points[Landmarks::RightInnerEye]);
            ptsStream << p2d.x << " " << p2d.y << "\n";
            p2d = converter.MeshToMapCoords(depthmap, l.points[Landmarks::Nosetip]);
            ptsStream << p2d.x << " " << p2d.y << "\n";

            ptsStream << "}\n";
        }
    }

    static void exportForVOSM(QString dirPath)
    {
        QDir dir(dirPath);
        QStringList filter; filter << "*.obj";
        QFileInfoList list = dir.entryInfoList(filter, QDir::Files);
        foreach (const QFileInfo &info, list)
        {
            Mesh m = Mesh::fromOBJ(info.absoluteFilePath());
            QString lPath = dirPath + QDir::separator() + info.baseName() + ".xml";
            Landmarks l(lPath);

            MapConverter converter;
            Map depthmap = SurfaceProcessor::depthmap(m, converter, 1.0);
            SurfaceProcessor::smooth(depthmap, 1, 10);
            CurvatureStruct curvature = SurfaceProcessor::calculateCurvatures(depthmap);

            QString imgPath = dirPath + QDir::separator() + info.baseName() + ".png";
            cv::imwrite(imgPath.toStdString(), curvature.curvatureIndex.toMatrix() * 255);

            QString ptsPath = dirPath + QDir::separator() + info.baseName() + ".pts";
            QFile ptsFile(ptsPath);
            ptsFile.open(QFile::WriteOnly);
            QTextStream ptsStream(&ptsFile);
            ptsStream << "version: 1\nn_points: " << l.points.size() << "\n{\n";
            foreach (const cv::Point3d &p, l.points)
            {
                cv::Point2d p2d = converter.MeshToMapCoords(depthmap, p);
                ptsStream << p2d.x << " " << p2d.y << "\n";
            }
            ptsStream << "}\n";
        }
    }

    static int  testLandmarkDetection(int argc, char *argv[], QString pathToOBJ)
    {
        Mesh face = Mesh::fromOBJ(pathToOBJ);
        LandmarkDetector detector(face);
        Landmarks landmarks = detector.Detect();


        QApplication app(argc, argv);
        GLWidget widget;
        widget.setWindowTitle("GL Widget");
        widget.addFace(&face);
        widget.addLandmarks(&landmarks);
        widget.show();

        return app.exec();
    }

    static int testHorizontalProfileLines(int argc, char *argv[])
    {
        Mesh face = Mesh::fromXYZFile("02463d652.abs.xyz", true);
        MapConverter converter;
        Map depth = SurfaceProcessor::depthmap(face, converter, 2);
        SurfaceProcessor::smooth(depth, 1, 20);
        depth.levelSelect(0);
        CurvatureStruct cs = SurfaceProcessor::calculateCurvatures(depth);

        QFile::remove("horizontalProfiles");
        QFile::remove("horizontalProfiles-d");
        QFile::remove("horizontalProfiles-dd");
        QFile::remove("curvatureIndex");
        QFile::remove("curvatureGauss");
        QFile::remove("curvatureMean");
        for (int i = 10; i < depth.h; i += 50)
        {
            MaskedVector vec = depth.horizontalProfile(i);
            vec.savePlot("horizontalProfiles", true);

            MaskedVector d = vec.derivate();
            d.savePlot("horizontalProfiles-d", true);

            MaskedVector dd = d.derivate();
            dd.savePlot("horizontalProfiles-dd", true);

            MaskedVector index = cs.curvatureIndex.horizontalProfile(i);
            index.savePlot("curvatureIndex", true);

            MaskedVector gauss = cs.curvatureGauss.horizontalProfile(i);
            gauss.savePlot("curvatureGauss", true);

            MaskedVector mean = cs.curvatureMean.horizontalProfile(i);
            mean.savePlot("curvatureMean", true);
        }

        Matrix indexMatrix = cs.curvatureIndex.toMatrix(0, 0, 1)*255;
        cv::imwrite("indexMatrix.png", indexMatrix);
    }

    static void testCombine()
    {
        Mesh face = Mesh::fromXYZFile("02463d652.abs.xyz", true);
        MapConverter converter;
        Map depth = SurfaceProcessor::depthmap(face, converter, 2);
        SurfaceProcessor::smooth(depth, 1, 20);
        CurvatureStruct cs = SurfaceProcessor::calculateCurvatures(depth);

        Matrix depththMat = depth.toMatrix();
        Matrix curvatureMat = cs.curvatureIndex.toMatrix();
        Matrix resultMat = (0.5*depththMat + 0.5*curvatureMat);
        cv::imshow("combination", resultMat);
        cv::waitKey(0);

        resultMat *= 255;
        cv::imwrite("combination.png", resultMat);
    }

    static void testAnotation(const QString &dirPath, bool uniqueIDsOnly)
    {
        FaceFeaturesAnotation::anotateOBJ(dirPath, uniqueIDsOnly);
    }

    static void testGoodAnotation(const QString &dirPath)
    {
        QDir dir(dirPath, "*.xml");
        QFileInfoList entries = dir.entryInfoList();
        foreach (const QFileInfo &e, entries)
        {
            Landmarks l(e.absoluteFilePath());
            qDebug() << "Checking:" << e.absoluteFilePath();
            if (!l.check())
            {
                qDebug() << e.fileName() << "didn't pass the landmark.check()";
                return;
            }
        }
        qDebug() << "All" << entries.count() << "entries passed";
    }
};

#endif // TESTFACEFEATURESDETECTION_H