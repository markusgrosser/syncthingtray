#ifndef SYNCTHINGTRAY_NO_WEBVIEW
#include "./webpage.h"
#include "./webviewdialog.h"

#include "../application/settings.h"

#include "../../connector/syncthingconnection.h"

#include "resources/config.h"

#include <QDesktopServices>
#include <QAuthenticator>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#if defined(SYNCTHINGTRAY_USE_WEBENGINE)
# include <QWebEngineSettings>
# include <QWebEngineView>
# include <QWebEngineCertificateError>
#elif defined(SYNCTHINGTRAY_USE_WEBKIT)
# include <QWebSettings>
# include <QWebView>
# include <QWebFrame>
# include <QSslError>
# include <QNetworkRequest>
#endif

using namespace Data;

namespace QtGui {

WebPage::WebPage(WebViewDialog *dlg, WEB_VIEW_PROVIDER *view) :
    WEB_PAGE_PROVIDER(view),
    m_dlg(dlg),
    m_view(view)
{
#ifdef SYNCTHINGTRAY_USE_WEBENGINE
    settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);
    connect(this, &WebPage::authenticationRequired, this, static_cast<void(WebPage::*)(const QUrl &, QAuthenticator *)>(&WebPage::supplyCredentials));
#else
    settings()->setAttribute(QWebSettings::JavascriptCanOpenWindows, true);
    setNetworkAccessManager(&Data::networkAccessManager());
    connect(&Data::networkAccessManager(), &QNetworkAccessManager::authenticationRequired, this, static_cast<void(WebPage::*)(QNetworkReply *, QAuthenticator *)>(&WebPage::supplyCredentials));
    connect(&Data::networkAccessManager(), &QNetworkAccessManager::sslErrors, this, static_cast<void(WebPage::*)(QNetworkReply *, const QList<QSslError> &errors)>(&WebPage::handleSslErrors));
#endif

    if(!m_view) {
        // initialization for new window
        // -> delegate to external browser if no view is assigned
#ifdef SYNCTHINGTRAY_USE_WEBENGINE
        connect(this, &WebPage::urlChanged, this, &WebPage::delegateNewWindowToExternalBrowser);
#else
        connect(this->mainFrame(), &QWebFrame::urlChanged, this, &WebPage::delegateNewWindowToExternalBrowser);
#endif
        // -> there need to be a view, though
        m_view = new WEB_VIEW_PROVIDER;
        m_view->setPage(this);
    }
}

bool WebPage::isSamePage(const QUrl &url1, const QUrl &url2)
{
    if(url1.scheme() == url2.scheme()
            && url1.host() == url2.host()
            && url1.port() == url2.port()) {
        QString path1 = url1.path();
        while(path1.endsWith(QChar('/'))) {
            path1.resize(path1.size() - 1);
        }
        QString path2 = url2.path();
        while(path2.endsWith(QChar('/'))) {
            path2.resize(path2.size() - 1);
        }
        if(path1 == path2) {
            return true;
        }
    }
    return false;
}

WEB_PAGE_PROVIDER *WebPage::createWindow(WEB_PAGE_PROVIDER::WebWindowType type)
{
    Q_UNUSED(type)
    return new WebPage;
}

#ifdef SYNCTHINGTRAY_USE_WEBENGINE
bool WebPage::certificateError(const QWebEngineCertificateError &certificateError)
{
    switch(certificateError.error()) {
    case QWebEngineCertificateError::CertificateCommonNameInvalid:
    case QWebEngineCertificateError::CertificateAuthorityInvalid:
        // FIXME: only ignore the error if the used certificate matches the certificate
        // known to be used by the Syncthing GUI
        return true;
    default:
        return false;
    }
}

bool WebPage::acceptNavigationRequest(const QUrl &url, WEB_PAGE_PROVIDER::NavigationType type, bool isMainFrame)
{
    Q_UNUSED(isMainFrame)
    Q_UNUSED(type)
    return handleNavigationRequest(this->url(), url);
}

#else // SYNCTHINGTRAY_USE_WEBKIT
bool WebPage::acceptNavigationRequest(QWebFrame *frame, const QNetworkRequest &request, WEB_PAGE_PROVIDER::NavigationType type)
{
    Q_UNUSED(frame)
    Q_UNUSED(type)
    return handleNavigationRequest(mainFrame() ? mainFrame()->url() : QUrl(), request.url());
}
#endif

void WebPage::delegateNewWindowToExternalBrowser(const QUrl &url)
{
    QDesktopServices::openUrl(url);
    // this page and the associated view are useless
    m_view->deleteLater();
    deleteLater();
}

void WebPage::supplyCredentials(const QUrl &requestUrl, QAuthenticator *authenticator)
{
    Q_UNUSED(requestUrl)
    supplyCredentials(authenticator);
}

void WebPage::supplyCredentials(QNetworkReply *reply, QAuthenticator *authenticator)
{
    Q_UNUSED(reply)
    supplyCredentials(authenticator);
}

void WebPage::supplyCredentials(QAuthenticator *authenticator)
{
    if(m_dlg && m_dlg->settings().authEnabled) {
        authenticator->setUser(m_dlg->settings().userName);
        authenticator->setPassword(m_dlg->settings().password);
    }
}

bool WebPage::handleNavigationRequest(const QUrl &currentUrl, const QUrl &targetUrl)
{
    if(currentUrl.isEmpty()) {
        // allow initial request
        return true;
    }
    // only allow navigation on the same page
    if(isSamePage(currentUrl, targetUrl)) {
        return true;
    }
    // otherwise open URL in external browser
    QDesktopServices::openUrl(targetUrl);
    return false;
}

#ifdef SYNCTHINGTRAY_USE_WEBKIT
void WebPage::handleSslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
    Q_UNUSED(errors)
    if(m_dlg && reply->request().url().host() == m_view->url().host()) {
        reply->ignoreSslErrors(m_dlg->settings().expectedSslErrors);
    }
}
#endif

}

#endif // SYNCTHINGTRAY_NO_WEBVIEW
