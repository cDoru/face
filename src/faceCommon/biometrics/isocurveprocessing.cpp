#include "isocurveprocessing.h"

#include <QDir>
#include <QFileInfoList>

#include "linalg/serialization.h"
#include "facelib/surfaceprocessor.h"

IsoCurveProcessing::IsoCurveProcessing()
{
}

QVector<SubjectIsoCurves> IsoCurveProcessing::readDirectory(const QString &path, const QString &separator, const QString &fileNameFilter)
{
    QDir dir(path, fileNameFilter);
    QFileInfoList files = dir.entryInfoList();

    QVector<SubjectIsoCurves> result;
    foreach(const QFileInfo &fileInfo, files)
    {
        SubjectIsoCurves subject;

        subject.subjectID = fileInfo.baseName().split(separator)[0].toInt();
        subject.vectorOfIsocurves = Serialization::readVectorOfPointclouds(fileInfo.absoluteFilePath());

        result << subject;
    }

    return result;
}

void IsoCurveProcessing::sampleIsoCurves(QVector<SubjectIsoCurves> &data, int modulo)
{
    for (int i = 0; i < data.count(); i++)
    {
        SubjectIsoCurves &subj = data[i];
        VectorOfIsocurves newIsoCurves;
        for (int j = 0; j < subj.vectorOfIsocurves.count(); j += modulo)
        {
            newIsoCurves << subj.vectorOfIsocurves[j];
        }
        subj.vectorOfIsocurves = newIsoCurves;
    }
}


void IsoCurveProcessing::sampleIsoCurvePoints(QVector<SubjectIsoCurves> &data, int modulo)
{
    for (int i = 0; i < data.count(); i++)
    {
        SubjectIsoCurves &subj = data[i];
        VectorOfIsocurves newIsoCurves;
        for (int j = 0; j < subj.vectorOfIsocurves.count(); j++)
        {
            VectorOfPoints &isocurve = subj.vectorOfIsocurves[j];
            VectorOfPoints newIsoCurve;
            for (int k = 0; k < isocurve.count(); k += modulo)
            {
                newIsoCurve << isocurve[k];
            }
            newIsoCurves << newIsoCurve;
        }
        subj.vectorOfIsocurves = newIsoCurves;
    }
}

void IsoCurveProcessing::selectIsoCurves(QVector<SubjectIsoCurves> &data, int start, int end)
{
    for (int i = 0; i < data.count(); i++)
    {
        SubjectIsoCurves &subj = data[i];
        VectorOfIsocurves newIsoCurves;
        for (int j = start; j < end; j++)
        {
            newIsoCurves << subj.vectorOfIsocurves[j];
        }
        subj.vectorOfIsocurves = newIsoCurves;
    }
}

void IsoCurveProcessing::stats(QVector<SubjectIsoCurves> &data)
{
    assert(data.count() > 0);
    assert(data[0].vectorOfIsocurves.count() > 0);
    assert(data[0].vectorOfIsocurves[0].count() > 0);

    int samplesCount = data[0].vectorOfIsocurves[0].count();
    int curvesCount = data[0].vectorOfIsocurves.count();
    int subjectCount = data.count();

    for (int curveIndex = 0; curveIndex < curvesCount; curveIndex++)
    {
        bool allSamplesValid = true;
        for (int subjectIndex = 0; subjectIndex < subjectCount; subjectIndex++)
        {
            for (int sampleIndex = 0; sampleIndex < samplesCount; sampleIndex++)
            {
                cv::Point3d &p = data[subjectIndex].vectorOfIsocurves[curveIndex][sampleIndex];
                if (p.x != p.x || p.y != p.y || p.z != p.z)
                {
                    allSamplesValid = false;
                }
            }
        }

        qDebug() << "curveIndex:" << curveIndex << "All samples valid:" << allSamplesValid;
    }
}

QVector<Template> IsoCurveProcessing::generateTemplates(QVector<SubjectIsoCurves> &data)
{
    QVector<Template> result;
    foreach (const SubjectIsoCurves &subjectIsoCurves, data)
    {
        Template t;
        t.subjectID = subjectIsoCurves.subjectID;

        QVector<double> fv;
        foreach (const VectorOfPoints &isocurve, subjectIsoCurves.vectorOfIsocurves)
        {
            foreach (const cv::Point3d &p, isocurve)
            {
                fv << p.x;
                fv << p.y;
                fv << p.z;
            }
        }

        t.featureVector = Vector(fv);
        result << t;
    }

    return result;
}

QVector<Template> IsoCurveProcessing::generateEuclDistanceTemplates(QVector<SubjectIsoCurves> &data)
{
    QVector<Template> result;
    foreach (const SubjectIsoCurves &subjectIsoCurves, data)
    {
        Template t;
        t.subjectID = subjectIsoCurves.subjectID;

        QVector<double> fv;
        foreach (const VectorOfPoints &isocurve, subjectIsoCurves.vectorOfIsocurves)
        {
            fv << SurfaceProcessor::isoGeodeticCurveToEuclDistance(isocurve, cv::Point3d(0,0,0));
        }

        t.featureVector = Vector(fv);
        result << t;
    }

    return result;
}
