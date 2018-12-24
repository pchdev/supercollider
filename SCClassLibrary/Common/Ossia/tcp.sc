WebSocketConnection
{
	var m_ptr;
	var m_address;
	var m_port;
	var textMessageCallback;
	var binaryMessageCallback;
	var oscMessageCallback;
	var httpMessageCallback;

	*new { |ptr, addr, port|
		^this.newCopyArgs(ptr, addr, port).prmBind();
	}

	prmBind {
		_WebSocketConnectionBind
		^this.primitiveFailed
	}

	onTextMessageReceived { |callback|
		textMessageCallback = callback;
	}

	onBinaryMessageReceived { |callback|
		binaryMessageCallback = callback;
	}

	onOSCMessageReceived { |callback|
		oscMessageCallback = callback;
	}

	onHTTPReplyReceived { |callback|
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

	pvOnHTTPMessageReceived { |message|
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

	writeRaw { |data|
		_WebSocketConnectionWriteRaw
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

	onTextMessageReceived { |callback|
		m_connection.onTextMessageReceived(callback);
	}

	onBinaryMessageReceived { |callback|
		m_connection.onBinaryMessageReceived(callback);
	}

	onOSCMessageReceived { |callback|
		m_connection.onOSCMessageReceived(callback);
	}

	onHTTPReplyReceived { |callback|
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

	writeRaw { |data|
		m_connection.writeRaw(data);
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

	onNewConnection { |callback|
		m_ncb = callback;
	}

	onDisconnection { |callback|
		m_dcb = callback;
	}

	onHTTPRequestReceived { |callback|
		m_hcb = callback;
	}

	pvOnNewConnection { |connection, addr, port|
		var con = WebSocketConnection(connection, addr, port);
		m_connections = m_connections.add(connection);
		m_ncb.value(connection, addr, port)
	}

	pvOnHTTPRequestReceived { |request|
		m_hcb.value(request);
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



