#include "RedmineClient.hpp"

#include "KeyAuthenticator.hpp"
#include "PasswordAuthenticator.hpp"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>

RedmineClient::RedmineClient() {}

RedmineClient::RedmineClient ( QString base_url )
{
	this->setBaseUrl ( base_url );
}

RedmineClient::RedmineClient ( QString base_url, QString apiKey, bool checkSsl, QObject* parent ) : QObject ( parent )
{
	( void ) checkSsl;
	this->setBaseUrl ( base_url );
	this->setAuth ( apiKey );
}

RedmineClient::RedmineClient ( QString base_url, QString login, QString password, bool checkSsl, QObject* parent ) : QObject ( parent )
{
	( void ) checkSsl;
	this->setBaseUrl ( base_url );
	this->setAuth ( login, password );
}

void RedmineClient::setBaseUrl ( QString base_url )
{
	this->_base_url = base_url;
}
void RedmineClient::setBaseUrl ( const char *base_url )
{
	this->_base_url = QString::fromUtf8 ( base_url );
}

QString RedmineClient::getBaseUrl()
{
	return this->_base_url;
}

void RedmineClient::setAuth ( QString apiKey )
{
	this->_authenticator = new KeyAuthenticator ( apiKey.toLatin1() );
	init();
}

void RedmineClient::setAuth ( QString login, QString password )
{
	this->_authenticator = new PasswordAuthenticator ( login, password );
	init();
}

RedmineClient::~RedmineClient()
{
	if ( this->_nma != NULL ) {
		delete this->_nma;
		this->_nma = NULL;
	}

	if ( this->_authenticator != NULL ) {
		delete this->_authenticator;
		this->_authenticator = NULL;
	}
}

void RedmineClient::requestFinished_wrapper ( QNetworkReply *reply )
{
	if ( this->_callbacks.contains ( reply ) ) {
		struct callback *cb;
		cb = &this->_callbacks[reply];
		QByteArray    data_raw  = reply->readAll();
#ifdef DEBUG
		qDebug ( "Raw data: %s", data_raw.data() );
#endif

		if ( cb->format != JSON )
			qFatal ( "The only supported format it JSON" );

		QJsonDocument data_json = QJsonDocument::fromJson ( ( ( QString ) data_raw ).toUtf8() );
		this->requestFinished ( cb->obj_ptr, cb->funct, reply, &data_json, cb->arg );

		if ( cb->free_arg )
			if ( cb->arg != NULL )
				free ( cb->arg );

		this->_callbacks.remove ( reply );
	}

	return;
}

void RedmineClient::init()
{
	this->_nma       = new QNetworkAccessManager;
	this->_ua        = "redmine-qt";
	connect ( this->_nma, SIGNAL ( finished ( QNetworkReply * ) ), this, SLOT ( requestFinished_wrapper ( QNetworkReply * ) ) );
	return;
}

void RedmineClient::setUserAgent ( QByteArray ua )
{
	this->_ua = ua;
	return;
}

QNetworkReply *RedmineClient::sendRequest ( QString uri,
        EFormat format,
        EMode   mode,
        void *obj_ptr,
        callback_t callback,
        void *callback_arg,
        bool  free_arg,
        const QString    &getParams,
        const QByteArray &requestData
                                          )
{
	QByteArray postDataSize = QByteArray::number ( requestData.size() );
	QString url_str = this->_base_url + "/" + uri + "." + ( format == JSON ? "json" : "xml" ) + "?" + getParams;
	QUrl    url     = url_str;
#ifdef DEBUG
	qDebug ( "URL: %s (%s)", url_str.toLatin1().data() );
#endif

	if ( !url.isValid() ) {
		qDebug ( "Invalid URL: %s", url_str.toLatin1().data() );
		return NULL;
	}

	QNetworkRequest request ( url );
	request.setRawHeader ( "User-Agent",          _ua );
	request.setRawHeader ( "X-Custom-User-Agent", _ua );
	request.setRawHeader ( "Content-Type",        ( format == JSON ? "application/json" : "text/xml" ) );
	request.setRawHeader ( "Content-Length",      postDataSize );
	this->_authenticator->addAuthentication ( &request );
	QNetworkReply* reply = NULL;

	switch ( mode ) {
		case GET:
			reply = this->_nma->get ( request );
			break;

		case POST:
			reply = this->_nma->post ( request, requestData );
			break;

		case PUT:
			reply = this->_nma->put ( request, requestData );
			break;

		case DELETE:
			reply = this->_nma->deleteResource ( request );
			break;
	}

	if ( ( reply != NULL ) && ( callback != NULL ) ) {
		this->_callbacks[reply].free_arg = free_arg;
		this->_callbacks[reply].arg      = callback_arg;
		this->_callbacks[reply].format   = format;
		this->_callbacks[reply].obj_ptr  = obj_ptr;
		this->_callbacks[reply].funct    = callback;
	} else {
		if ( ( callback_arg != NULL ) && free_arg )
			free ( callback_arg );
	}

	return reply;
}
