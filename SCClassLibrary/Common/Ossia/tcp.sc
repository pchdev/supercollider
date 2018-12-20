TcpConnection
{
	var m_ptr;
	var m_read_callback;

	*new { |ptr|
		^this.newCopyArgs(ptr);
	}

	onDataReceived { |data|
		m_read_callback.value(data);
	}

	setReadCallback { |cb|
		m_read_callback = cb;
	}

	write { |data|
		prmWrite(data);
	}

	prmWrite { |data|
		_TcpConnectionWrite
		^this.primitiveFailed
	}
}

TcpClient
{
	var m_ptr;
	var m_host_addr;
	var m_connected_callback;
	var m_connection;
	classvar g_instances;

	*new {
		^this.newCopyArgs(0x0).tcpClientCtor().stackNew();
	}

	*initClass {
		g_instances = [];
		ShutDown.add({
			postln("TcpClient cleanup");
			g_instances.do(_.free());
		})
	}

	stackNew {
		g_instances = g_instances.add(this);
	}

	tcpClientCtor
	{
		m_ptr = this.prmInstantiate();
		m_ptr.postln;
	}

	prmInstantiate {
		_TcpClientInstantiate
		^this.primitiveFailed
	}

	connectToHost { |hostAddr, port|
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
		"new_connection".postln;
		m_connected_callback.value();
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
	var m_connections;
	var m_nconnection_callback;
	classvar g_instances;

	*new { |port|
		^this.newCopyArgs(0x0, port).tcpServerCtor().stackNew();
	}

	*initClass {
		g_instances = [];
		ShutDown.add({
			postln("TcpServer cleanup");
			g_instances.do(_.free());
		})
	}

	stackNew {
		g_instances = g_instances.add(this);
	}

	tcpServerCtor
	{
		m_ptr = this.prmInstantiateRun();
		m_connections = [];
	}

	onNewConnection { |connection|
		var con = TcpConnection(connection);
		m_connections = m_connections.add(con);
		m_nconnection_callback.value(con);
	}

	onDisconnection { |connection|

	}

	prmInstantiateRun
	{
		_TcpServerInstantiateRun
		^this.primitiveFailed
	}

	prmGetNewConnection
	{
		_TcpServerGetNewConnection
		^this.primitiveFailed
	}

	prmGetDisconnection
	{
		_TcpServerGetDisconnection
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
	var m_tcp_connection;

	*new { |tcpConnection|
		^this.newCopyArgs(tcpConnection)
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



