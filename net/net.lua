net = {
	engine,
	_process_packet = nil,    --����������
	_on_accept = nil,         --�����µ�����
	_on_connect = nil,
	_on_disconnect = nil,     --�������ӹر�
	_send_timeout = nil,      
	_recv_timeout = nil, 
}

function net:listen(ip,port)
	Listen(self.engine,ip,port)
	return self
end

function net:connect(ip,port,timeout)
	Connect(self.engine,ip,port,timeout)
end

function net:process_packet(connection,rpacket)
	if self._process_packet then
		self._process_packet(connection,rpacket)
	end
end

function net:on_accept(connection)
	print("on_accept")
	if self._on_accept then
		print("a client come")
		self._on_accept(connection)
	end
end

function net:on_connect(connection)
	if self._on_connect then
		self._on_connect(connection)
	end
end

function net:on_disconnect(connection)
	if self._on_disconnect then
		self._on_disconnect(connection)
	end
end

function net:send_timeout(connection)
	if self._send_timeout then
		self._send_timeout(connection)
	end
end

function net:recv_timeout(connection)
	if self._recv_timeout then
		self._recv_timeout(connection)
	end
end

function net:finalize()
	netservice_delete(self.engine)
	print("net destroy")
end

function net:new()
  local o = {}   
  self.__gc = net.finalize
  self.__index = self
  setmetatable(o, self)
  o.engine = netservice_new(o)
  return o
end

function net:run(timeout)
	if self.engine == nil then
		return -1
	end
	return EngineRun(self.engine,timeout)
end
