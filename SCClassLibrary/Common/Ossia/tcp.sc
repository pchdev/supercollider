WebSocketConnection
{
	var m_ptr;
	var <address;
	var <port;
	var textMessageCallback;
	var binaryMessageCallback;
	var oscMessageCallback;
	var httpMessageCallback;

	*new { |ptr|
		^this.newCopyArgs(ptr).prmBind();
	}

	prmBind {
		_WebSocketConnectionBind
		^this.primitiveFailed
	}

	onTextMessageReceived_ { |callback|
		textMessageCallback = callback;
	}

	onBinaryMessageReceived_ { |callback|
		binaryMessageCallback = callback;
	}

	onOSCMessageReceived_ { |callback|
		oscMessageCallback = callback;
	}

	onHttpReplyReceived_ { |callback|
		httpMessageCallback = callback;
	}

	// prim-callbacks ---------------------------

	pvOnTextMessageReceived { |message|
		textMessageCallback.value(message);
	}

	pvOnBinaryMessageReceived { |message|
		binaryMessageCallback.value(message);
	}

	pvOnOSCMessageReceived { |address, arguments|
		oscMessageCallback.value(address, arguments);
	}

	pvOnHttpMessageReceived { |message|
		httpMessageCallback.value(message);
	}

	writeText { |msg|
		_WebSocketConnectionWriteText
		^this.primitiveFailed
	}

	writeOSC { |addr, arguments|
		_WebSocketConnectionWriteOSC
		^this.primitiveFailed
	}

	writeBinary { |data|
		_WebSocketConnectionWriteBinary
		^this.primitiveFailed
	}
}

WebSocketClient
{
	var m_ptr;
	var m_connection;
	var <connected;
	var m_ccb; // connected
	var m_dcb; // disconnected

	classvar g_instances;

	*initClass {
		g_instances = [];
		ShutDown.add({
			g_instances.do(_.free());
		})
	}

	// CREATE -------------------------------

	*new {
		^this.new.wsClientCtor();
	}

	wsClientCtor {
		connected = false;
		g_instances = g_instances.add(this);
	}

	primCreate {
		_WebSocketClientCreate
		^this.primitiveFailed
	}

	// CONNECTION/DISCONNECTION ------------------------------

	connect { |hostAddr, port|
		_WebSocketClientConnect
		^this.primitiveFailed
	}

	disconnect {
		_WebSocketClientDisconnect
		^this.primitiveFailed
	}

	onConnected { |callback|
		m_ccb = callback;
	}

	onDisconnected { |callback|
		m_dcb = callback;
	}

	pvOnConnected { |connection, addr, port|
		m_connection = WebSocketConnection(connection, addr, port);
		connected = true;
		m_ccb.value(addr, port);
	}

	pvOnDisconnected {
		connected = false;
		m_dcb.value();
		m_connection = nil;
	}

	// CALLBACKS -----------------------------

	onTextMessageReceived_ { |callback|
		m_connection.onTextMessageReceived(callback);
	}

	onBinaryMessageReceived_ { |callback|
		m_connection.onBinaryMessageReceived(callback);
	}

	onOSCMessageReceived_ { |callback|
		m_connection.onOSCMessageReceived(callback);
	}

	onHTTPReplyReceived_ { |callback|
		m_connection.onHTTPReplyReceived(callback);
	}

	// WRITING -------------------------------

	writeText { |msg|
		m_connection.writeText(msg);
	}

	writeOSC { |addr ...arguments|
		m_connection.writeOSC(addr, arguments);
	}

	writeBinary { |data|
		m_connection.writeBinary(data);
	}

	request { |req|
		_WebSocketClientRequest
		^this.primitiveFailed
	}

	free {
		g_instances.remove(this);
		m_connection = nil;
		this.primFree();
	}

	primFree {
		_WebSocketClientFree
		^this.primitiveFailed
	}
}

Http
{
	*ok { ^200 }
	*notFound { ^404 }
}

HttpRequest
{
	var m_ptr;
	var <>method;
	var <>query;
	var <>mime;
	var <>body;

	*newFromPrim { |ptr|
		^this.newCopyArgs(ptr).reqCtor()
	}

	reqCtor {
		_HttpRequestBind
		^this.primitiveFailed
	}

	*new { |server, method, query, mime, body|
		^this.newCopyArgs(0x0, method, query, mime, body)
	}

	reply { |code, mime, text|
		_HttpReply
		^this.primitiveFailed
	}

	replyJson { |json|
		// we assume code is 200 here
		this.reply(200, "application/json", json);
	}
}

WebSocketServer
{
	var m_ptr;
	var m_port;
	var m_connections;
	var m_ncb;
	var m_dcb;
	var m_hcb;

	classvar g_instances;

	*initClass {
		g_instances = [];
		ShutDown.add({
			postln("TCP-cleanup");
			g_instances.do(_.free());
		})
	}

	*new { |port|
		^this.newCopyArgs(0x0, port).wsServerCtor();
	}

	wsServerCtor {
		g_instances = g_instances.add(this);
		this.prmInstantiateRun(m_port);
	}

	prmInstantiateRun { |port|
		_WebSocketServerInstantiateRun
		^this.primitiveFailed
	}

	at { |index|
		^m_connections[index];
	}

	writeAll { |data|
		m_connections.do(_.write(data));
	}

	onNewConnection_ { |callback|
		m_ncb = callback;
	}

	onDisconnection_ { |callback|
		m_dcb = callback;
	}

	onHttpRequestReceived_ { |callback|
		m_hcb = callback;
	}

	pvOnNewConnection { |con|
		var connection = WebSocketConnection(con);
		m_connections = m_connections.add(connection);
		m_ncb.value(connection)
	}

	pvOnHttpRequestReceived { |request|
		var screq = HttpRequest.newFromPrim(request);
		m_hcb.value(screq);
	}

	pvOnDisconnection { |connection|

	}

	free {
		g_instances.remove(this);
		this.prmFree();
	}

	prmFree {
		_WebSocketServerFree
		^this.primitiveFailed
	}
}



