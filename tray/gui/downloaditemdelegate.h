#ifndef DOWNLOADITEMDELEGATE_H
#define DOWNLOADITEMDELEGATE_H

#include <QStyledItemDelegate>
#include <QPixmap>

namespace QtGui {

class DownloadItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    DownloadItemDelegate(QObject *parent);

    void paint(QPainter *, const QStyleOptionViewItem &, const QModelIndex &) const;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;

private:
    const QPixmap m_folderIcon;
};

}

#endif // DOWNLOADITEMDELEGATE_H
