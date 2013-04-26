#include "map.h"

#include <opencv2/core/core.hpp>

#include "linalg/matrixconverter.h"

Map::Map()
{
    w = 0;
    h = 0;
}

void Map::init(int _w, int _h)
{
    w = _w;
    h = _h;
    int n = w*h;
    flags = QVector<bool>(n, false);
    values = QVector<double>(n, 0.0);
}

Map::Map(int w, int h)
{
    init(w, h);
}

Map::Map(const Map &src)
{
    w = src.w;
    h = src.h;
    flags = src.flags;
    values = src.values;
}

/*Map::~Map()
{
    if (flags != NULL) delete [] flags;
    if (values != NULL) delete [] values;
}*/

void Map::setAll(double v)
{
    int n = w*h;
    for (int i = 0; i < n; i++)
    {
        set(i, v);
    }
}

void Map::unsetAll()
{
    int n = w*h;
    for (int i = 0; i < n; i++)
    {
        unset(i);
    }
}

void Map::levelSelect(double zLevel)
{
    int n = w*h;
    for (int i = 0; i < n; i++)
    {
        if (flags[i] && values[i] < zLevel)
        {
            unset(i);
        }
    }
}

void Map::linearScale(double multiply, double add)
{
    int n = w*h;
    for (int i = 0; i < n; i++)
    {
        if (flags[i])
        {
            values[i] = values[i] * multiply + add;
        }
    }
}

void Map::erode(int kernelSize)
{
    assert(kernelSize % 2 == 1);
    assert(kernelSize >= 3);
    int range = kernelSize/2;

    int count = 0;
    int toRemoveX[w*h];
    int toRemoveY[w*h];

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            if (!isSet(x, y)) continue;

            bool notValid = false;
            for (int y2 = y-range; y2 <= y+range; y2++)
            {
                for (int x2 = x-range; x2 <= x+range; x2++)
                {
                    if (!isValidCoord(x2, y2))
                        continue;

                    if (!isSet(x2, y2))
                    {
                        notValid = true;
                        toRemoveX[count] = x;
                        toRemoveY[count] = y;
                        count++;
                        break;
                    }
                }
                if (notValid) break;
            }
        }
    }

    for (int i = 0; i < count; i++)
    {
        unset(toRemoveX[i], toRemoveY[i]);
    }
}

double Map::minValue() const
{
    double min = 1e300;
    int n = w*h;
    for (int i = 0; i < n; i++)
    {
        if (flags[i] && values[i] < min)
        {
            min = values[i];
        }
    }
    return min;
}

double Map::maxValue() const
{
    double max = -1e300;
    int n = w*h;
    for (int i = 0; i < n; i++)
    {
        if (flags[i] && values[i] > max)
        {
            max = values[i];
        }
    }
    return max;
}

void Map::add(Map &other)
{
    assert(w == other.w);
    assert(h == other.h);
    int n = w*h;
    for (int i = 0; i < n; i++)
    {
        if (flags[i] && other.flags[i])
        {
            values[i] += other.values[i];
        }
        else
        {
            flags[i] = false;
        }
    }
}

void Map::linearTransform(double multiply, double add)
{
    int n = w*h;
    for (int i = 0; i < n; i++)
    {
        if (flags[i])
        {
            values[i] = multiply*values[i] + add;
        }
    }
}

MaskedVector Map::horizontalProfile(int y) const
{
    MaskedVector profile(w, 0.0, false);

    for (int x = 0; x < w; x++)
    {
        if (isSet(x, y))
        {
            profile.set(x, get(x, y));
        }
    }
    return profile;
}

MaskedVector Map::verticalProfile(int x) const
{
    MaskedVector profile(h, 0.0, false);

    for (int y = 0; y < h; y++)
    {
        if (isSet(x, y))
        {
            profile.set(y, get(x, y));
        }
    }
    return profile;
}

MaskedVector Map::meanVerticalProfile() const
{
    MaskedVector profile(h, 0.0, false);

    for (int y = 0; y < h; y++)
    {
        MaskedVector horizontal = horizontalProfile(y);
        if (horizontal.flagCount() > 0)
        {
            double value = horizontal.mean();
            profile.set(y, value);
        }
    }
    return profile;
}

MaskedVector Map::maxVerticalProfile() const
{
    MaskedVector profile(h, 0.0, false);

    for (int y = 0; y < h; y++)
    {
        MaskedVector horizontal = horizontalProfile(y);
        if (horizontal.flagCount() > 0)
        {
            double value = horizontal.max();
            profile.set(y, value);
        }
    }
    return profile;
}

MaskedVector Map::medianVerticalProfile() const
{
    MaskedVector profile(h, 0.0, false);

    for (int y = 0; y < h; y++)
    {
        MaskedVector horizontal = horizontalProfile(y);
        if (horizontal.flagCount() > 0)
        {
            double value = horizontal.median();
            profile.set(y, value);
        }
    }
    return profile;
}

MaskedVector Map::horizontalPointDensity(int y, int stripeWidth) const
{
    MaskedVector curve(w, 0.0, true);

    for (int x = 0; x < w; x++)
    {
        int count = 0;
        for (int y2 = y-stripeWidth; y2 <= y+stripeWidth; y2++)
        {
            if (isValidCoord(x, y2) && isSet(x, y2))
            {
                count++;
            }
        }
        curve.set(x, count);
    }

    return curve;
}

Map Map::densityMap(int kernelSize, bool fromCenter) const
{
    assert(kernelSize % 2 == 1);
    assert(kernelSize >= 3);
    int range = kernelSize/2;

    Map density(this->w, this->h);
    double windowCount = kernelSize*kernelSize;

    double fromCenterToBorder = sqrt(w/2*w/2 + h/2*h/2);

    for (int y = 0; y < h; y++)
    {
        for(int x = 0; x < w; x++)
        {
            int count = 0;
            for (int y2 = y-range; y2 <= y+range; y2++)
            {
                for (int x2 = x-range; x2 <= x+range; x2++)
                {
                    if (!isValidCoord(x2, y2)) continue;
                    if (!isSet(x2, y2)) continue;

                    count++;
                }
            }
            double value = count/windowCount;

            if (fromCenter)
            {
                double toCenter = sqrt((x-(w/2))*(x-(w/2)) + (y-(h/2))*(y-(h/2)));
                double factor = 1 - (toCenter/fromCenterToBorder);
                value = factor*value;
            }

            density.set(x, y, value);
        }
    }
    return density;
}

int Map::maxIndex() const
{
    double max = -1e300;
    int index = -1;
    int n = w*h;
    for (int i = 0; i < n; i++)
    {
        if (flags[i] && values[i] > max)
        {
            max = values[i];
            index = i;
        }
    }

    return index;
}

Matrix Map::toMatrix(double voidValue, double min, double max) const
{
    Matrix result(h, w);
    if (min == 0 && max == 0)
    {
        min = minValue();
        max = maxValue();
    }
    double delta = max - min;
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            if (isSet(x, y))
            {
                double val = (get(x, y)-min)/delta;
                //if (val > max) val = max;
                //if (val < min) val = min;
                result(y, x) = val; //*255;
            }
            else
            {
                result(y, x) = voidValue;
            }
        }
    }

    return result;
}

Map Map::fromMatrix(Matrix &matrix, double voidValue)
{
    Map result(matrix.cols, matrix.rows);
    for (int y = 0; y < matrix.rows; y++)
    {
        for (int x = 0; x < matrix.cols; x++)
        {
            double val = matrix(y,x);
            if (val != voidValue)
                result.set(x, y, val);
            else
                result.unset(x, y);
        }
    }
    return result;
}

void Map::getCropParams(int &startx, int &width, int &starty, int &height) const
{
    startx = w;
    starty = h;
    int endx = 0;
    int endy = 0;
    width = 0;
    height = 0;
    for (int x = 0; x < w; x++)
    {
        for (int y = 0; y < h; y++)
        {
            if (isSet(x,y))
            {
                if (x < startx) startx = x;
                if (y < starty) starty = y;

                if (x > endx) endx = x;
                if (y > endy) endy = y;
            }
        }
    }

    assert(endx > startx);
    assert(endy > starty);
    width = endx - startx;
    height = endy - starty;
}

Map Map::subMap(int startx, int width, int starty, int height) const
{
    Map newmap(width, height);
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int oldx = startx+x;
            int oldy = starty+y;

            if (oldx >= w || oldx < 0 || oldy >= h || oldy < 0)
                continue;

            if (isSet(oldx, oldy))
                newmap.set(x, y, get(oldx, oldy));
        }
    }
    return newmap;
}

QVector<double> Map::getUsedValues() const
{
    QVector<double> result;

    int n = flags.count();
    for (int i = 0; i < n; i++)
    {
        if (flags[i])
        {
            result << values[i];
        }
    }

    return result;
}

void Map::applyFilter(Matrix &kernel, int times, bool checkSum)
{
    assert(kernel.rows % 2 == 1);
    assert(kernel.cols % 2 == 1);

    int kernelAnchorX = kernel.cols/2;
    int kernelAnchorY = kernel.rows/2;
    int kernelW = kernel.cols/2;
    int kernelH = kernel.rows/2;

    for (int i = 0; i < times; i++)
    {
        QVector<double> newValues(w*h);
        for (int y = 0; y < this->h; y++)
        {
            for (int x = 0; x < this->w; x++)
            {
                if (!isSet(x, y)) continue;

                double newValue = 0;
                double sumOfKernelValues = 0;
                for (int yy = -kernelH; yy <= kernelH; yy++)
                {
                    for (int xx = -kernelW; xx <= kernelW; xx++)
                    {
                        if (!isValidCoord(x+xx, y+yy)) continue;
                        if (!isSet(x+xx, y+yy)) continue;

                        double kernelValue = kernel(cv::Point(kernelAnchorX+xx,kernelAnchorY+yy));
                        newValue += get(x+xx, y+yy)*kernelValue;
                        sumOfKernelValues += kernelValue;
                    }
                }

                if (checkSum)
                {
                    newValue = newValue/sumOfKernelValues;
                }
                newValues[coordToIndex(x, y)] = newValue;
            }
        }

        values = newValues;
    }
    /*Matrix src = this->toMatrix();
    cv::filter2D()*/
}

Map::Map(const QString &path)
{
    cv::FileStorage fs(path.toStdString(), cv::FileStorage::READ);
    w = (int)fs["w"];
    h = (int)fs["h"];

    cv::FileNode flagsNode = fs["flags"];
    std::vector<bool> stdFlags;
    //flagsNode >> stdFlags;
    flags = QVector<bool>::fromStdVector(stdFlags);

    cv::FileNode valuesNode = fs["values"];
    std::vector<double> stdValues;
    valuesNode >> stdValues;
    values = QVector<double>::fromStdVector(stdValues);
}

void Map::serialize(const QString &path)
{
    cv::FileStorage fs(path.toStdString(), cv::FileStorage::WRITE);
    fs << "w" << w;
    fs << "h" << h;
    int n = w * h;

    fs << "flags" << "[";
    for (int i = 0; i < n; i++)
    {
        fs << flags[i];
    }
    fs << "]";

    fs << "values" << "[";
    for (int i = 0; i < n; i++)
    {
        fs << values[i];
    }
    fs << "]";
}
