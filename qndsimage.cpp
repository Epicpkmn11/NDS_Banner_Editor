#include "qndsimage.h"

QNDSImage::QNDSImage() {}

QNDSImage::QNDSImage(const QImage& img, const QVector<u16>& pal, bool col0Transparent) {
    replace(img, pal, col0Transparent);
}

QNDSImage::QNDSImage(const QImage& img, int colorCount, int alphaThreshold) {
    replace(img, colorCount, alphaThreshold);
}

QNDSImage::QNDSImage(const QVector<u8>& ncg, const QVector<u16>& ncl, bool is4bpp, bool col0Transparent) {
    replace(ncg, ncl, is4bpp, col0Transparent);
}

u16 QNDSImage::toRgb15(u32 rgb24)
{
    u8 r, g, b;

    r = (rgb24 >> 16) & 0xFF;
    g = (rgb24 >> 8) & 0xFF;
    b = (rgb24 >> 0) & 0xFF;

    r >>= 3;
    g >>= 3;
    b >>= 3;

    return (b << 10) | (g << 5) | r;
}

u32 QNDSImage::toRgb24(u16 rgb15)
{
    u8 r, g, b;

    r = (rgb15 >> 0) & 0x1F;
    g = (rgb15 >> 5) & 0x1F;
    b = (rgb15 >> 10) & 0x1F;

    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);

    return (r << 16) | (g << 8) | b;
}

void QNDSImage::replace(const QImage& img, const QVector<u16>& pal, bool col0Transparent)
{
    this->col0Transparent = col0Transparent;

    palette = pal;

    const int newPalSize = pal.size();
    QVector<QColor> newPal24(newPalSize);
    for (int i = 0; i < newPalSize; i++)
        newPal24[i] = toRgb24(pal[i]);

    const int width = img.width();
    const int height = img.height();

    texture.resize(width * height);

    for (int i = 0, y = 0; y < height; y++)
        for (int x = 0; x < width; x++, i++)
            texture[i] = closestMatch(img.pixel(x, y), newPal24);

    texture = getTiled(width / 8, true);
}

void QNDSImage::replace(const QImage& img, int colorCount, int alphaThreshold)
{
    this->col0Transparent = alphaThreshold == 0;

    const int width = img.width();
    const int height = img.height();

    QVector<QColor> pal;
    pal.append(QColor(0, 0, 0, 0)); //Make transparent the first color

    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++)
        {
            QColor c = img.pixelColor(x, y);
            if(c.alpha() < alphaThreshold)
                c = QColor(0, 0, 0, 0);
            if(!pal.contains(c))
                pal.append(c);
        }
    }

    QVector<u16> newPal = createPalette(pal, colorCount);
    replace(img, newPal, col0Transparent);
}

void QNDSImage::replace(const QVector<u8>& ncg, const QVector<u16>& ncl, bool is4bpp, bool col0Transparent)
{
    this->col0Transparent = col0Transparent;

    if(is4bpp)
    {
        const int texSize = ncg.size() << 1;
        texture.resize(texSize);

        for (int i = 0, j = 0; i < texSize; i += 2, j++)
        {
            texture[i] = ncg[j] & 0xF;
            texture[i + 1] = (ncg[j] >> 4) & 0xF;
        }
    }
    else
        texture = ncg;

    palette = ncl;
}

QVector<u8> QNDSImage::getTiled(int tileWidth, bool inverse)
{
    const int textureSize = texture.size();
    const int width = tileWidth * 8;
    const int height = textureSize / width;

    QVector<u8> out(textureSize);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
        {
            u32 bf = x + y * width;

            u32 tf = bf % 64;
            u32 tx = tf % 8;
            u32 ty = tf / 8;

            u32 gf = bf / 64;
            u32 gx = (gf % tileWidth) * 8;
            u32 gy = (gf / tileWidth) * 8;

            u32 dsti, srci;
            if (inverse)
            {
                dsti = x + y * width;
                srci = (gx + tx) + (gy + ty) * width;
            }
            else
            {
                dsti = (gx + tx) + (gy + ty) * width;
                srci = x + y * width;
            }
            out[dsti] = texture[srci];
        }
    }

    return out;
}

#define _QNDSIMAGE_PLTT_IMAGE_DEBUG 0

QImage QNDSImage::toImage(int tileWidth)
{
#if _QNDSIMAGE_PLTT_IMAGE_DEBUG
    QImage out(16, 16, QImage::Format_ARGB32);
    out.fill(Qt::transparent);
    for (int i = 0, y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++, i++)
        {
            if (i == palette.size())
                break;
            out.setPixelColor(x, y, toRgb24(palette[i]));
        }
    }
    return out;
#else
    const int width = tileWidth * 8;
    const int height = texture.size() / width;

    QImage out(width, height, QImage::Format_ARGB32);
    out.fill(Qt::transparent);

    QVector<u8> tiled = getTiled(tileWidth, false);
    for(int i = 0, y = 0; y < height; y++) {
        for(int x = 0; x < width; x++, i++)
        {
            int colorIndex = tiled[i];
            if(!(colorIndex == 0 && col0Transparent))
            {
                QColor c = toRgb24(palette[colorIndex]);
                out.setPixelColor(x, y, c);
            }
        }
    }

    return out;
#endif
}

void QNDSImage::toNitro(QVector<u8>& ncg, QVector<u16>& ncl, bool is4bpp)
{
    if(is4bpp)
    {
        const int texSize = texture.size() >> 1;
        ncg.resize(texSize);

        for (int i = 0, j = 0; i < texSize; i++, j += 2)
        {
            ncg[i] = texture[j] & 0xF;
            ncg[i] |= (texture[j + 1] & 0xF) << 4;
        }
    }
    else
        ncg = texture;

    ncl = palette;
}

QVector<u16> QNDSImage::createPalette(const QVector<QColor>& pal, int colorCount)
{
    QVector<QColor> palCopy = pal;

    // For finding color channel that has the most wide range,
    // we need to keep their lower and upper bound.
    int lower_red = palCopy[0].red(),
        lower_green = palCopy[0].green(),
        lower_blue = palCopy[0].blue();
    int upper_red = 0,
        upper_green = 0,
        upper_blue = 0;

    // Loop trough all the colors
    for (QColor c : palCopy)
    {
        lower_red = std::min(lower_red, c.red());
        lower_green = std::min(lower_green, c.green());
        lower_blue = std::min(lower_blue, c.blue());

        upper_red = std::max(upper_red, c.red());
        upper_green = std::max(upper_green, c.green());
        upper_blue = std::max(upper_blue, c.blue());
    }

    int red = upper_red - lower_red;
    int green = upper_green - lower_green;
    int blue = upper_blue - lower_blue;
    int max = std::max(std::max(red, green), blue);

    // Compare two rgb color according to our selected color channel.
    std::sort(palCopy.begin(), palCopy.end(),
    [max, red, green/*, blue*/](const QColor& c1, const QColor& c2)
    {
        if (max == red)  // if red is our color that has the widest range
            return c1.red() < c2.red(); // just compare their red channel
        else if (max == green) //...
            return c1.green() < c2.green();
        else //if (max == blue)
            return c1.blue() < c2.blue();
    });

    QList<QList<QColor>> lists;
    int listSize = palCopy.size() / colorCount;

    for (int i = 0; i < colorCount; ++i)
    {
        QList<QColor> list;
        for (int j = listSize * i; j < (listSize * i) + listSize; ++j)
            list.append(palCopy[j]);
        lists.append(list);
    }

    QVector<u16> palette;
    for (QList<QColor> list : lists)
    {
        QColor c = list[list.size() / 2];
        palette.append(toRgb15(c.rgb()));
    }

    return palette;
}

inline int QNDSImage::pixelDistance(QColor p1, QColor p2)
{
    int r1 = p1.red();
    int g1 = p1.green();
    int b1 = p1.blue();
    int a1 = p1.alpha();

    int r2 = p2.red();
    int g2 = p2.green();
    int b2 = p2.blue();
    int a2 = p2.alpha();

    return abs(r1 - r2) + abs(g1 - g2) + abs(b1 - b2) + abs(a1 - a2);
}

inline int QNDSImage::closestMatch(QColor pixel, const QVector<QColor>& clut)
{
    int idx = 0;
    int current_distance = INT_MAX;
    for (int i = 0; i < clut.size(); ++i)
    {
        int dist = pixelDistance(pixel, clut[i]);
        if (dist < current_distance)
        {
            current_distance = dist;
            idx = i;
        }
    }
    return idx;
}
