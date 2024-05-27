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

#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QStyleOptionSlider>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QStyle>
#include <QtWidgets/QDial>
#include <QtGui/QPainter>
#include <QtCore/QMap>
#include <QtCore/QTimer>
#include <QtCore/QVector>
#include <QtCore/QSettings>
#include <QtCore/QAbstractItemModel>

#include "dialog.h"
#include "matrix.h"

namespace {

const int resolution = 1000;

}


class Matrix::WeightItemDelegate:
    public QStyledItemDelegate
{

public:
    WeightItemDelegate(QObject *parent = 0):
        QStyledItemDelegate(parent)
    {

    }


    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        QDial *dial = new QDial(parent);
        dial->setRange(0, resolution);
        dial->setSingleStep(1);
        dial->setPageStep(resolution / 10);

        return dial;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        const qreal d = index.data(Qt::DisplayRole).toReal();
        const int v = qBound(0, qRound(d * resolution), resolution);

        QStyleOptionSlider opt;
        *static_cast<QStyleOption *>(&opt) = *static_cast<const QStyleOption *>(&option);

        opt.orientation = Qt::Vertical;
        opt.minimum = 0;
        opt.maximum = resolution;
        opt.sliderPosition = v;
        opt.sliderValue = v;
        opt.singleStep = 1;
        opt.pageStep = resolution / 10;
        opt.upsideDown = true;
        opt.notchTarget = 3.7;
        opt.dialWrapping = false;
        opt.tickInterval = 1;
        opt.tickPosition = QSlider::TicksAbove;
        opt.subControls = QStyle::SC_All;
        opt.subControls &= ~QStyle::SC_DialTickmarks;
        opt.activeSubControls = QStyle::SC_None;

        painter->fillRect(opt.rect, index.data(Qt::BackgroundRole).value<QColor>());

        painter->save();

#if QT_VERSION <= QT_VERSION_CHECK(5, 15, 1)
        // Work around a bug in Qt's drawing routine
        opt.rect.translate(-opt.rect.topLeft());
        painter->translate(option.rect.topLeft());
#endif

        const QWidget *widget = option.widget;
        QStyle *style = widget ? widget->style() : QApplication::style();
        style->drawComplexControl(QStyle::CC_Dial, &opt, painter);

        painter->restore();
    }


    void setEditorData(QWidget *editor, const QModelIndex &index) const
    {
        QDial *dial = static_cast<QDial *>(editor);

        const qreal d = index.data(Qt::EditRole).toReal();
        const int v = qBound(0, qRound(d * resolution), resolution);
        dial->setValue(v);
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
    {
        QDial *dial = static_cast<QDial *>(editor);

        const qreal d = qreal(dial->value()) / qreal(resolution);
        model->setData(index, d);
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        Q_UNUSED(index);
        QDial *dial = static_cast<QDial *>(editor);
        dial->setGeometry(option.rect);
    }

};


class Matrix::MatrixModel:
    public QAbstractItemModel
{
public:
    MatrixModel(QAbstractItemModel *parent):
        QAbstractItemModel(parent),

        m_source(parent)
    {
        connect(m_source, &QAbstractItemModel::modelAboutToBeReset, this, &MatrixModel::beginResetModel);
        connect(m_source, &QAbstractItemModel::modelReset, this, &MatrixModel::endResetModel);

        connect(m_source, &QAbstractItemModel::rowsAboutToBeInserted, this, &MatrixModel::beginInsertColumns);
        connect(m_source, &QAbstractItemModel::rowsInserted, this, &MatrixModel::endInsertColumns);
        connect(m_source, &QAbstractItemModel::rowsAboutToBeMoved, this, &MatrixModel::beginMoveColumns);
        connect(m_source, &QAbstractItemModel::rowsMoved, this, &MatrixModel::endMoveColumns);
        connect(m_source, &QAbstractItemModel::rowsAboutToBeRemoved, this, &MatrixModel::beginRemoveColumns);
        connect(m_source, &QAbstractItemModel::rowsRemoved, this, &MatrixModel::endRemoveColumns);
    }

    ~MatrixModel()
    {

    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const
    {
        if (parent.isValid())
            return 0;

        return m_source->rowCount();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const
    {
        if (parent.isValid())
            return 0;

        return scenes.count();
    }

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const
    {
        if (parent.isValid())
            return QModelIndex();
        else if (row < 0 || row >= scenes.count())
            return QModelIndex();
        else if (column < 0 || column >= columnCount())
            return QModelIndex();

        return createIndex(row, column, nullptr);
    }

    QModelIndex parent(const QModelIndex &child) const
    {
        Q_UNUSED(child);
        return QModelIndex();
    }

    Qt::ItemFlags flags(const QModelIndex &index) const
    {
        if (!index.isValid())
            return Qt::NoItemFlags;

        return Qt::ItemIsEnabled | Qt::ItemIsEditable;
    }

    QVariant data(const QModelIndex &index, int role) const
    {
        if (!index.isValid())
            return QVariant();
        else if (index.row() < 0 || index.row() >= scenes.count())
            return QVariant();

        const QString id = headerData(index.column(), Qt::Horizontal).toString();
        const Scene &s = scenes.at(index.row());
        switch (role) {
        case Qt::EditRole:
            return s.weights.value(id, 0.0);

        case Qt::DisplayRole:
            return s.weights.value(id, 0.0);

        case Qt::BackgroundRole:
            return QColor::fromRgbF(s.lots.value(id, 0.0), 0.0, 0.0, 0.7);

        default:
            return QVariant();
        }
    }

    bool setData(const QModelIndex &index, const QVariant &value, int role)
    {
        if (!index.isValid())
            return false;
        if (role != Qt::EditRole)
            return false;

        bool ok;
        double weight = value.toDouble(&ok);
        if (!ok)
            return false;

        const QString id = headerData(index.column(), Qt::Horizontal).toString();
        Scene &s = scenes[index.row()];
        s.weights.insert(id, weight);

        return true;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const
    {
        if (orientation == Qt::Horizontal) {
            const QModelIndex index = m_source->index(section, Dialog::IdColumn);
            return index.data(role);
        }
        else {
            const Scene &scene = scenes.at(section);
            switch (role) {
            case Qt::DisplayRole:
                return scene.title;

            case Qt::ToolTipRole:
                return QString("Szene Nr. %1 (%2)").arg(QString::number(scene.scene), formatDuration(scene.duration));

            default:
                return QVariant();
            }
        }
    }

    bool removeRows(int row, int count, const QModelIndex &parent)
    {
        if (parent.isValid())
            return false;
        if (count <= 0)
            return false;
        else if (row < 0 || row + count > scenes.count())
            return false;

        beginRemoveRows(QModelIndex(), row, row + count - 1);
        scenes.remove(row, count);
        endRemoveRows();
        return true;
    }

    void addScene(const QString &title, int scene, int duration = 0)
    {
        const int row = scenes.count();
        beginInsertRows(QModelIndex(), row, row);
        scenes.append(Scene(title, scene, duration));
        endInsertRows();
    }

    void load(QSettings &settings)
    {
        beginResetModel();
        scenes.clear();

        const int count = settings.beginReadArray("matrix");
        for (int i = 0; i < count; i++) {
            settings.setArrayIndex(i);
            const QVariant vscene = settings.value("scene", QVariant());
            if (vscene.isNull())
                continue;

            bool ok;
            const int scene = vscene.toInt(&ok);
            if (!ok || scene < 0)
                continue;

            Scene s(
                settings.value("title").toString(),
                scene,
                settings.value("duration").toDouble()
            );

            settings.beginGroup("weights");
            for (int j = 0; j < columnCount(); j++) {
                const QString id = headerData(j, Qt::Horizontal).toString();

                bool ok;
                const double weight = settings.value(id).toDouble(&ok);
                if (ok)
                    s.weights.insert(id, weight);
            }
            settings.endGroup();

            scenes.append(s);
        }

        settings.endArray();
        endResetModel();
    }

    void save(QSettings &settings)
    {
        settings.beginWriteArray("matrix", scenes.count());
        for (int i = 0; i < scenes.count(); i++) {
            const Scene &scene = scenes.at(i);
            settings.setArrayIndex(i);
            settings.setValue("title", scene.title);
            settings.setValue("scene", scene.scene);
            settings.setValue("duration", scene.duration);

            settings.beginGroup("weights");
            for (int j = 0; j < columnCount(); j++) {
                const QString id = headerData(j, Qt::Horizontal).toString();
                if (scene.weights.contains(id))
                    settings.setValue(id, scene.weights.value(id));
            }
            settings.endGroup();
        }
        settings.endArray();
    }


    void resetLots()
    {
        for (Scene &scene: scenes)
            scene.lots.clear();
    }

    void accumulateLots()
    {
        for (Scene &scene: scenes) {
            for (QMap<QString, double>::const_iterator it = scene.weights.cbegin();
                 it != scene.weights.cend();
                 ++it) {

                QMap<QString, double>::iterator l = scene.lots.find(it.key());
                if (l == scene.lots.cend())
                    l = scene.lots.insert(it.key(), 0.0);

                *l += *it / 10.0;

                if (*l >= 1.0) {
                    *l = 0.0;

                    if (Dialog::instance()) {
                        Dialog::instance()->startScene(
                            it.key().toInt(),
                            scene.scene);

                    }

                    qDebug() << scene.title << it.key();
                }
            }
        }
    }

protected:


private:
    struct Scene {
        Scene(const QString &t, int s, int d = 0):
            title(t),
            scene(s),
            duration(d)
        { }

        QString title;
        int scene;
        int duration;

        QMap<QString, double> weights;

        QMap<QString, double> lots;
    };

    QString formatDuration(int duration) const
    {
        int mins = duration / 60;
        int secs = duration % 60;

        return QStringLiteral("%1:%2")
            .arg(mins)
            .arg(secs, 2, 10, QLatin1Char('0'));
    }

    QAbstractItemModel *m_source;
    QVector<Scene> scenes;
};



Matrix::Matrix(Dialog *parent):
    QDialog(parent)
{
    QSettings settings;

    setupUi(this);
    tableView->setItemDelegate( new WeightItemDelegate(this) );

    m_model = new MatrixModel(parent->model());
    tableView->setModel(m_model);
    m_model->load(settings);

    connect(
        addSceneButton, SIGNAL(clicked(bool)),
        this, SLOT(addScene())
    );
    connect(
        removeSceneButton, SIGNAL(clicked(bool)),
        this, SLOT(removeScene())
    );

    activator = new QTimer(this);
    activator->setSingleShot(false);
    connect(
        runBox, SIGNAL(toggled(bool)),
        this, SLOT(activate(bool))
    );
    connect(
        activator, SIGNAL(timeout()),
        this, SLOT(activatorTimeout())
    );
    connect(
        speedSlider, SIGNAL(valueChanged(int)),
        this, SLOT(setSpeed(int))
    );
}

Matrix::~Matrix()
{
    QSettings settings;
    m_model->save(settings);
}

void Matrix::changeEvent(QEvent *e)
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

void Matrix::addScene()
{
    bool ok;
    const QString title = QInputDialog::getText(this, "Szene hinzufügen", "Titel:",
                                                QLineEdit::Normal, QString(), &ok);
    if (!ok) return;

    const int scene = QInputDialog::getInt(this, "Szene hinzufügen", "Szene-Nr.:",
                                           1, 0, UINT16_MAX, 1, &ok);
    if (!ok) return;

    m_model->addScene(title, scene);
}

void Matrix::removeScene()
{
    if (tableView->currentIndex().isValid())
        m_model->removeRow(tableView->currentIndex().row());
}

void Matrix::activate(bool active)
{
    if (active) {
        setSpeed(speedSlider->value());
    }
    else {
        activator->stop();
    }
}

void Matrix::activatorTimeout()
{
    m_model->accumulateLots();
    tableView->viewport()->update();
}

void Matrix::setSpeed(int s)
{
    activator->stop();
    activator->start(s * 10);
}
