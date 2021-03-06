#include "./trayicon.h"
#include "./traywidget.h"

#include "../application/settings.h"

#include "../../connector/syncthingconnection.h"

#include <qtutilities/misc/dialogutils.h>

#include <QCoreApplication>
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmap>

using namespace std;
using namespace Dialogs;
using namespace Data;
#ifdef QT_UTILITIES_SUPPORT_DBUS_NOTIFICATIONS
using namespace MiscUtils;
#endif

namespace QtGui {

/*!
 * \brief Instantiates a new tray icon.
 */
TrayIcon::TrayIcon(QObject *parent) :
    QSystemTrayIcon(parent),
    m_initialized(false),
    m_size(QSize(128, 128)),
    m_statusIconDisconnected(QIcon(renderSvgImage(QStringLiteral(":/icons/hicolor/scalable/status/syncthing-disconnected.svg")))),
    m_statusIconIdling(QIcon(renderSvgImage(QStringLiteral(":/icons/hicolor/scalable/status/syncthing-ok.svg")))),
    m_statusIconScanning(QIcon(renderSvgImage(QStringLiteral(":/icons/hicolor/scalable/status/syncthing-default.svg")))),
    m_statusIconNotify(QIcon(renderSvgImage(QStringLiteral(":/icons/hicolor/scalable/status/syncthing-notify.svg")))),
    m_statusIconPause(QIcon(renderSvgImage(QStringLiteral(":/icons/hicolor/scalable/status/syncthing-pause.svg")))),
    m_statusIconSync(QIcon(renderSvgImage(QStringLiteral(":/icons/hicolor/scalable/status/syncthing-sync.svg")))),
    m_statusIconError(QIcon(renderSvgImage(QStringLiteral(":/icons/hicolor/scalable/status/syncthing-error.svg")))),
    m_statusIconErrorSync(QIcon(renderSvgImage(QStringLiteral(":/icons/hicolor/scalable/status/syncthing-error-sync.svg")))),
    m_trayMenu(this),
    m_status(SyncthingStatus::Disconnected)
#ifdef QT_UTILITIES_SUPPORT_DBUS_NOTIFICATIONS
    ,
    m_disconnectedNotification(QCoreApplication::applicationName(), QStringLiteral("network-disconnect"), 5000),
    m_internalErrorNotification(QCoreApplication::applicationName() + tr(" - internal error"), NotificationIcon::Critical, 5000),
    m_syncthingNotification(tr("Syncthing notification"), NotificationIcon::Warning, 10000),
    m_syncCompleteNotification(QCoreApplication::applicationName(), NotificationIcon::Information, 5000)
#endif
{
    // set context menu
    connect(m_contextMenu.addAction(QIcon::fromTheme(QStringLiteral("internet-web-browser"), QIcon(QStringLiteral(":/icons/hicolor/scalable/apps/internet-web-browser.svg"))), tr("Web UI")), &QAction::triggered, m_trayMenu.widget(), &TrayWidget::showWebUi);
    connect(m_contextMenu.addAction(QIcon::fromTheme(QStringLiteral("preferences-other"), QIcon(QStringLiteral(":/icons/hicolor/scalable/apps/preferences-other.svg"))), tr("Settings")), &QAction::triggered, m_trayMenu.widget(), &TrayWidget::showSettingsDialog);
    connect(m_contextMenu.addAction(QIcon::fromTheme(QStringLiteral("folder-sync"), QIcon(QStringLiteral(":/icons/hicolor/scalable/actions/folder-sync.svg"))), tr("Rescan all")), &QAction::triggered, &m_trayMenu.widget()->connection(), &SyncthingConnection::rescanAllDirs);
    connect(m_contextMenu.addAction(QIcon::fromTheme(QStringLiteral("text-x-generic"), QIcon(QStringLiteral(":/icons/hicolor/scalable/mimetypes/text-x-generic.svg"))), tr("Log")), &QAction::triggered, m_trayMenu.widget(), &TrayWidget::showLog);
    m_contextMenu.addMenu(m_trayMenu.widget()->connectionsMenu());
    connect(m_contextMenu.addAction(QIcon::fromTheme(QStringLiteral("help-about"), QIcon(QStringLiteral(":/icons/hicolor/scalable/apps/help-about.svg"))), tr("About")), &QAction::triggered, m_trayMenu.widget(), &TrayWidget::showAboutDialog);
    m_contextMenu.addSeparator();
    connect(m_contextMenu.addAction(QIcon::fromTheme(QStringLiteral("window-close"), QIcon(QStringLiteral(":/icons/hicolor/scalable/actions/window-close.svg"))), tr("Close")), &QAction::triggered, this, &TrayIcon::deleteLater);
    setContextMenu(&m_contextMenu);

#ifdef QT_UTILITIES_SUPPORT_DBUS_NOTIFICATIONS
    // setup notifications
    m_disconnectedNotification.setMessage(tr("Disconnected from Syncthing"));
    m_disconnectedNotification.setActions(QStringList(tr("Try to reconnect")));
    connect(&m_disconnectedNotification, &DBusNotification::actionInvoked, &(m_trayMenu.widget()->connection()), static_cast<void(SyncthingConnection::*)(void)>(&SyncthingConnection::connect));
    m_syncthingNotification.setActions(QStringList({QStringLiteral("show"), tr("Show"), QStringLiteral("dismiss"), tr("Dismiss")}));
    connect(&m_syncthingNotification, &DBusNotification::actionInvoked, this, &TrayIcon::handleSyncthingNotificationAction);
#endif

    // set initial status
    updateStatusIconAndText(SyncthingStatus::Disconnected);

    // connect signals and slots
    SyncthingConnection *connection = &(m_trayMenu.widget()->connection());
    connect(this, &TrayIcon::activated, this, &TrayIcon::handleActivated);
    connect(this, &TrayIcon::messageClicked, m_trayMenu.widget(), &TrayWidget::dismissNotifications);
    connect(connection, &SyncthingConnection::error, this, &TrayIcon::showInternalError);
    connect(connection, &SyncthingConnection::newNotification, this, &TrayIcon::showSyncthingNotification);
    connect(connection, &SyncthingConnection::statusChanged, this, &TrayIcon::updateStatusIconAndText);

    m_initialized = true;
}

/*!
 * \brief Moves the specified \a point in the specified \a rect.
 */
void moveInside(QPoint &point, const QRect &rect)
{
    if(point.y() < rect.top()) {
        point.setY(rect.top());
    } else if(point.y() > rect.bottom()) {
        point.setY(rect.bottom());
    }
    if(point.x() < rect.left()) {
        point.setX(rect.left());
    } else if(point.x() > rect.right()) {
        point.setX(rect.right());
    }
}

void TrayIcon::handleActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch(reason) {
    case QSystemTrayIcon::Context:
        // can't catch that event on Plasma 5 anyways
        break;
    case QSystemTrayIcon::MiddleClick:
        m_trayMenu.widget()->showWebUi();
        break;
    case QSystemTrayIcon::Trigger: {
        m_trayMenu.showAtCursor();
        break;
    }
    default:
        ;
    }
}

void TrayIcon::handleSyncthingNotificationAction(const QString &action)
{
    if(action == QLatin1String("dismiss")) {
        m_trayMenu.widget()->dismissNotifications();

    } else if(action == QLatin1String("show")) {
        m_trayMenu.widget()->showNotifications();
    }
}

void TrayIcon::showInternalError(const QString &errorMsg, SyncthingErrorCategory category)
{
    const auto &settings = Settings::values();
    if(settings.notifyOn.internalErrors
            && (m_trayMenu.widget()->connection().autoReconnectTries() < 1 || category != SyncthingErrorCategory::OverallConnection)) {
#ifdef QT_UTILITIES_SUPPORT_DBUS_NOTIFICATIONS
        if(settings.dbusNotifications) {
            m_internalErrorNotification.update(errorMsg);
        } else
#endif
        {
            showMessage(tr("Error"), errorMsg, QSystemTrayIcon::Critical);
        }
    }
}

void TrayIcon::showSyncthingNotification(ChronoUtilities::DateTime when, const QString &message)
{
    Q_UNUSED(when)
    const auto &settings = Settings::values();
    if(settings.notifyOn.syncthingErrors) {
#ifdef QT_UTILITIES_SUPPORT_DBUS_NOTIFICATIONS
        if(settings.dbusNotifications) {
            m_syncthingNotification.update(message);
        } else
#endif
        {
            showMessage(tr("Syncthing notification - click to dismiss"), message, QSystemTrayIcon::Warning);
        }
    }
    updateStatusIconAndText(m_status);
}

void TrayIcon::updateStatusIconAndText(SyncthingStatus status)
{
    if(m_initialized && m_status == status) {
        return;
    }

    const SyncthingConnection &connection = trayMenu().widget()->connection();
    const auto &settings = Settings::values();
    switch(status) {
    case SyncthingStatus::Disconnected:
        setIcon(m_statusIconDisconnected);
        if(connection.autoReconnectInterval() > 0) {
            setToolTip(tr("Not connected to Syncthing - trying to reconnect every %1 ms")
                       .arg(connection.autoReconnectInterval()));
        } else {
            setToolTip(tr("Not connected to Syncthing"));
        }
        if(m_initialized && settings.notifyOn.disconnect) {
#ifdef QT_UTILITIES_SUPPORT_DBUS_NOTIFICATIONS
            if(settings.dbusNotifications) {
                m_disconnectedNotification.show();
            } else
#endif
            {
                showMessage(QCoreApplication::applicationName(), tr("Disconnected from Syncthing"), QSystemTrayIcon::Warning);
            }
        }
        break;
    case SyncthingStatus::Reconnecting:
        setIcon(m_statusIconDisconnected);
        setToolTip(tr("Reconnecting ..."));
        break;
    default:
#ifdef QT_UTILITIES_SUPPORT_DBUS_NOTIFICATIONS
        m_disconnectedNotification.hide();
#endif
        if(connection.hasOutOfSyncDirs()) {
            if(status == SyncthingStatus::Synchronizing) {
                setIcon(m_statusIconErrorSync);
                setToolTip(tr("Synchronization is ongoing but at least one directory is out of sync"));
            } else {
                setIcon(m_statusIconError);
                setToolTip(tr("At least one directory is out of sync"));
            }
        } else if(connection.hasUnreadNotifications()) {
            setIcon(m_statusIconNotify);
            setToolTip(tr("Notifications available"));
        } else {
            switch(status) {
            case SyncthingStatus::Idle:
                setIcon(m_statusIconIdling);
                setToolTip(tr("Syncthing is idling"));
                break;
            case SyncthingStatus::Scanning:
                setIcon(m_statusIconScanning);
                setToolTip(tr("Syncthing is scanning"));
                break;
            case SyncthingStatus::Paused:
                setIcon(m_statusIconPause);
                setToolTip(tr("At least one device is paused"));
                break;
            case SyncthingStatus::Synchronizing:
                setIcon(m_statusIconSync);
                setToolTip(tr("Synchronization is ongoing"));
                break;
            default:
                ;
            }
        }
    }
    switch(status) {
    case SyncthingStatus::Disconnected:
    case SyncthingStatus::Reconnecting:
    case SyncthingStatus::Synchronizing:
        break;
    default:
        if(m_status == SyncthingStatus::Synchronizing && settings.notifyOn.syncComplete) {
            const vector<SyncthingDir *> &completedDirs = connection.completedDirs();
            if(!completedDirs.empty()) {
                QString message;
                if(completedDirs.size() == 1) {
                    message = tr("Synchronization of %1 complete").arg(completedDirs.front()->displayName());
                } else {
                    QStringList names;
                    names.reserve(static_cast<int>(completedDirs.size()));
                    for(const SyncthingDir *dir : completedDirs) {
                        names << dir->displayName();
                    }
                    message = tr("Synchronization of the following devices complete:\n") + names.join(QStringLiteral(", "));
                }
#ifdef QT_UTILITIES_SUPPORT_DBUS_NOTIFICATIONS
                if(settings.dbusNotifications) {
                    m_syncCompleteNotification.update(message);
                } else
#endif
                {
                    showMessage(QCoreApplication::applicationName(), message, QSystemTrayIcon::Information);
                }
            }
        }
    }
    m_status = status;
}

/*!
 * \brief Renders an SVG image to a QPixmap.
 * \remarks If instantiating QIcon directly from SVG image the icon is not displayed under Plasma 5. It would work
 *          with Tint2, tough.
 */
QPixmap TrayIcon::renderSvgImage(const QString &path)
{
    QSvgRenderer renderer(path);
    QPixmap pm(m_size);
    pm.fill(QColor(Qt::transparent));
    QPainter painter(&pm);
    renderer.render(&painter, pm.rect());
    return pm;
}

}
