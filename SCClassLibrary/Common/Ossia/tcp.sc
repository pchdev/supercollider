TcpConnection
{
	var m_ptr;
	var m_rcallback;

	*new { |ptr|
		^this.newCopyArgs(ptr);
	}

	onDataReceived { |data|
		m_rcallback.value(data);
	}

	setReadCallback { |cb|
		m_rcallback = cb;
	}

	write { |data|
		this.prmWrite(data);
	}

	prmWrite { |data|
		_TcpConnectionWrite
		^this.primitiveFailed
	}

	prmBind { |ptr|
		_TcpConnectionBind
		^this.primitiveFailed
	}
}

TcpClient
{
	var m_ptr;
	var m_host_addr;
	var m_ccallback;
	var m_rcallback;
	var m_connection;
	classvar g_instances;

	*new { |cfunc, rfunc|
		^this.newCopyArgs(0x0, "127.0.0.1", cfunc, rfunc).tcpClientCtor().stackNew();
	}

	*initClass {
		g_instances = [];
		ShutDown.add({
			g_instances.do(_.free());
		})
	}

	stackNew {
		g_instances = g_instances.add(this);
	}

	tcpClientCtor {
		this.prmInstantiate();
	}

	prmInstantiate {
		_TcpClientInstantiate
		^this.primitiveFailed
	}

	connect { |hostAddr, port|
		m_connection = this.prmConnect(hostAddr, port);
	}

	prmConnect { |hostAddr, port|
		_TcpClientConnect
		^this.primitiveFailed
	}

	disconnect {
		_TcpClientDisconnect
		^this.primitiveFailed
	}

	onConnected { |connection|
		m_connection = TcpConnection(connection);
		m_ccallback.value();
	}

	write { |data|
		m_connection.write(data);
	}

	free {
		g_instances.remove(this);
		this.prmFree();
	}

	prmFree {
		_TcpClientFree
		^this.primitiveFailed
	}
}

TcpServer
{
	var m_ptr;
	var m_port;
	var m_nconnection_callback;
	var m_dconnection_callback;
	var m_connections;
	classvar g_instances;

	*new { |port, cfunc, dfunc|
		^this.newCopyArgs(0x0, port, cfunc, dfunc).tcpServerCtor().stackNew();
	}

	*initClass {
		g_instances = [];
		ShutDown.add({
			postln("TCP-cleanup");
			g_instances.do(_.free());
		})
	}

	stackNew {
		g_instances = g_instances.add(this);
	}

	tcpServerCtor {
		m_connections = [];
		this.prmInstantiateRun(m_port);
	}

	onNewConnection { |connection|
		var con = TcpConnection(connection);
		m_connections = m_connections.add(con);
		m_nconnection_callback.value(con);
	}

	onDisconnection { |connection|

	}

	getConnection { |index|
		^m_connections[index];
	}

	prmInstantiateRun { |port|
		_TcpServerInstantiateRun
		^this.primitiveFailed
	}

	free {
		g_instances.remove(this);
		this.prmFree();
	}

	prmFree {
		_TcpServerFree
		^this.primitiveFailed
	}
}

WebSocketConnection
{
	var m_tcpcon;

	*new { |tcpcon|
		^this.newCopyArgs(tcpcon)
	}

	onTextMessageReceived
	{

	}

	onBinaryMessageReceived
	{

	}

	writeText
	{

	}

	writeBinary
	{

	}

}

WebSocketClient
{
	var m_host_addr;
	var m_tcp_client;
	var m_tcp_connection;

	*new { |hostAddr|
		^this.newCopyArgs(hostAddr);
	}

	connect { |hostAddr|
		m_tcp_client.connect(hostAddr);
	}

	disconnect { }

	writeText { |msg| }
	writeBinary { |msg| }

	onTextMessageReceived { }
	onBinaryMessageReceived { }

}

WebSocketServer
{
	var m_port;
	var m_connections;

	*new { |port|
		^this.newCopyArgs(port)
	}

	onNewConnection {}
	getConnection {}
	onDisconnection {}
}



