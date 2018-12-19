TcpConnection
{
	var m_ptr;

	*new { |ptr|
		^this.newCopyArgs(ptr);
	}

	onDataReceived { |data|

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

	*new { |hostAddr|
		^this.newCopyArgs(0x0, hostAddr).tcpClientCtor();
	}

	tcpClientCtor
	{

	}

	connect { |hostAddr|
		m_connection = prmConnect(hostAddr);
	}

	prmConnect { |hostAddr|
		_TcpClientConnect
		^this.primitiveFailed
	}

	disconnect {
		_TcpClientDisconnect
		^this.primitiveFailed
	}

	onConnected
	{
		m_connected_callback.value();
	}


}

TcpServer
{
	var m_ptr;
	var m_port;
	var m_connections;
	var m_nconnection_callback;

	*new { |port|
		^this.newCopyArgs(0x0, port).tcpServerCtor();
	}

	tcpServerCtor
	{
		m_ptr = this.prmInstantiateRun();
		m_connections = [];
	}

	onNewConnection
	{
		var con = TcpConnection(this.prmGetNewConnection());
		m_connections = m_connections.add(con);
		m_nconnection_callback.value(con);
	}

	onDisconnection
	{
		var con = this.prmGetDisconnection();
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



