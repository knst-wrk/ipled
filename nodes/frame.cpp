/** This file is part of ipled - a versatile LED stripe controller.
Copyright (C) 2024 Sven Pauli <sven@knst-wrk.de>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <QtDebug>

#include <QtWidgets/QAbstractItemDelegate>
#include <QtWidgets/QFileDialog>
#include <QtCore/QAbstractListModel>
#include <QtCore/QByteArray>
#include <QtCore/QSettings>
#include <QtCore/QVariant>
#include <QtCore/QVector>
#include <QtCore/QFile>
#include <QtGui/QPainter>

#include "dialog.h"
#include "frame.h"



class FramesModel:
    public QAbstractListModel
{
public:
    enum Roles {
        RawRole = Qt::UserRole + 1,
        Tpm2Role = Qt::UserRole + 2,
        Tpz2Role = Qt::UserRole + 3,
    };

    FramesModel(QObject *parent = 0):
        QAbstractListModel(parent)
    {

    }

    ~FramesModel()
    {

    }

    int rowCount(const QModelIndex &parent) const
    {
        if (parent.isValid())
            return 0;
        return frames.count();
    }

    QVariant data(const QModelIndex &index, int role) const
    {
        if (!index.isValid())
            return QVariant();
        else if (index.column() != 0)
            return QVariant();
        else if (index.row() >= frames.count())
            return QVariant();

        switch (role)
        {
        case Qt::DisplayRole:
            return QString();

        case RawRole:
            return frames.at(index.row());

        case Tpm2Role:
            return envelope(0xDA, frames.at(index.row()));

        case Tpz2Role:
            return envelope(0xCA, compress(frames.at(index.row())));

        default:
            return QVariant();
        }
    }


    bool load(const QString &file)
    {
        beginResetModel();
        frames.clear();

        QFile f(file);
        if (!f.open(QIODevice::ReadOnly))
            return false;

        while (!f.atEnd()) {
            const QByteArray sign = f.read(2);
            if (sign.count() != 2)
                break;
            else if ((unsigned char) sign.at(0) != 0xC9 || (unsigned char) sign.at(1) != 0xDA)
                break;

            const QByteArray len = f.read(2);
            if (len.count() != 2)
                break;

            const int length = ((unsigned char) len.at(0) << 8) + (unsigned char) len.at(1);
            const QByteArray uni = f.read(length);
            if (uni.count() != length)
                break;

            const QByteArray end = f.read(1);
            if (end.count() != 1)
                break;
            else if ((unsigned char) end.at(0) != 0x36)
                break;

            frames.append(uni);
        }

        f.close();
        endResetModel();
        return true;
    }

private:
    QVector<QByteArray> frames;

private:
    QByteArray envelope(uint8_t tag = 0xDA, const QByteArray &raw = QByteArray()) const
    {
        QByteArray e;
        e.resize(2 + 2 + raw.count() + 1);
        e[0] = 0xC9;
        e[1] = tag;

        quint16 len = raw.count();
        e[2] = (char) (len >> 8);
        e[3] = (char) (len >> 0);

        e.replace(4, raw.count(), raw);
        e[e.count() - 1] = 0x36;
        return e;
    }

    QByteArray compress(const QByteArray &a) const
    {
        QByteArray compressed;
        QDataStream s(&compressed, QIODevice::WriteOnly);

        int i = 0;
        while (i < a.count()) {
            s << (uint8_t) a.at(i++);
            if (i < 6)
                continue;

            if (a.mid(i - 6, 3) == a.mid(i - 3, 3)) {
                uint8_t n = 0;
                int j = i + 3;
                while (j < a.length()) {
                    if (a.mid(i - 3, 3) != a.mid(j - 3, 3))
                        break;

                    j += 3;
                    n++;
                    if (n == 250)
                        break;
                }

                s << n;
                i = j - 3;
            }

        }

        return compressed;
    }
};


class FrameItemDelegate:
    public QAbstractItemDelegate
{
public:
    FrameItemDelegate(QObject *parent = 0):
        QAbstractItemDelegate(parent)
    {

    }

    ~FrameItemDelegate()
    {

    }


    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        const QByteArray frame = index.data(FramesModel::RawRole).toByteArray();
        if (frame.isNull())
            return;

        painter->save();
        painter->fillRect(option.rect, Qt::black);
        if (option.state & QStyle::State_Selected) {
            painter->setPen(option.palette.highlight().color());
            painter->drawRect(option.rect.adjusted(0, 0, -1, -1));
        }

        painter->setRenderHint(QPainter::Antialiasing, true);
        QPen pen;
        pen.setWidthF((qreal) option.rect.width() / frame.count() * 2.0);
        const qreal top = (qreal) option.rect.top() + option.rect.height() * 0.3;
        const qreal bottom = (qreal) option.rect.bottom() - option.rect.height() * 0.3;
        for (int i = 0; (i + 2) < frame.count(); i += 3) {
            const int r = (unsigned char) frame.at(i + 0);
            const int g = (unsigned char) frame.at(i + 1);
            const int b = (unsigned char) frame.at(i + 2);
            pen.setColor(QColor::fromRgb(r, g, b));
            painter->setPen(pen);

            const qreal x = (qreal) option.rect.left() + (qreal) option.rect.width() * i / frame.count();
            painter->drawLine(QPointF(x, top), QPointF(x, bottom));
        }
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &/*option*/, const QModelIndex &/*index*/) const
    {
        return QSize(12, 12);
    }
};


Frame::Frame(QWidget *parent):
    QDialog(parent)
{
    QSettings settings;
    setupUi(this);

    framesModel = new FramesModel(this);
    framesView->setModel(framesModel);
    framesView->setItemDelegate(new FrameItemDelegate(this));

    const int files = settings.beginReadArray("frameFiles");
    for (int i = qMax(0, files - 10); i < files; i++) {
        settings.setArrayIndex(i);
        const QString file = settings.value("file").toString().trimmed();
        if (!file.isEmpty())
            addFile(file);
    }
    settings.endArray();
    fileComboBox->setCurrentIndex(-1);

    connect(
        fileButton, SIGNAL(clicked(bool)),
        this, SLOT(pickFile())
    );
    connect(
        fileComboBox, SIGNAL(activated(QString)),
        this, SLOT(loadFile(QString))
    );
    connect(
        framesView, SIGNAL(doubleClicked(QModelIndex)),
        this, SLOT(sendFrame(QModelIndex))
    );
}

Frame::~Frame()
{
    QSettings settings;
    const int files = fileComboBox->count();
    settings.beginWriteArray("frameFiles", files);
    for (int i = qMax(0, files - 10); i < files; i++) {
        settings.setArrayIndex(i);
        const QString file = fileComboBox->itemText(i);
        if (!file.isEmpty())
            settings.setValue("file", file);
    }
    settings.endArray();
}

void Frame::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi(this);
        break;

    default:
        break;
    }
}

void Frame::addFile(const QString &file)
{
    for (int i = 0; i < fileComboBox->count(); i++) {
        if (fileComboBox->itemText(i) == file) {
            fileComboBox->setCurrentIndex(i);
            return;
        }
    }

    while (fileComboBox->count() >= 10)
        fileComboBox->removeItem(0);

    fileComboBox->addItem(file);
    fileComboBox->setCurrentIndex(fileComboBox->count() - 1);
}

void Frame::loadFile(const QString &file)
{
    if (!file.isNull())
        framesModel->load(file);
}

void Frame::pickFile()
{
    QFileDialog d(this);
    d.setAcceptMode(QFileDialog::AcceptOpen);
    d.setFileMode(QFileDialog::ExistingFile);
    d.setNameFilter("TPM2-Dateien (*.tp2 *.tpz)");
    if (d.exec() != QDialog::Accepted)
        return;
    else if (d.selectedFiles().isEmpty())
        return;

    const QString file = d.selectedFiles().first();
    addFile(file);
    loadFile(file);
}

void Frame::sendFrame(const QModelIndex &index)
{
    const QByteArray frame = framesModel->data(index, FramesModel::Tpz2Role).toByteArray();
    if (frame.isNull())
        return;

    Dialog *d = qobject_cast<Dialog *>(parent());
    if (d)
        d->sendFrame(frame);
}

