--[[
对CluaSocket的一层lua封装,提供协程化的阻塞接口,使得在应用上以阻塞的方式调用
Recv,Send,Connect,Accept等接口,而实际是异步处理的
]]--

local Sche = require "lua.sche"
local LinkQue  = require "lua.linkque"
local socket = {}

function socket:new(domain,type,protocal)
  local o = {}
  self.__index = self      
  setmetatable(o,self)
  o.luasocket = CSocket.new1(o,domain,type,protocal)
  if not o.luasocket then
	return nil
  end
  o.errno = 0
  return o   
end

function socket:new2(sock)
  local o = {}
  self.__index = self          
  setmetatable(o, self)
  o.luasocket = CSocket.new2(o,sock)
  o.errno = 0 
  return o
end

--[[
关闭socket对象，同时关闭底层的luasocket对象,这将导致连接断开。
务必保证对产生的每个socket对象调用Close。
]]--
function socket:Close()
	local luasocket = self.luasocket
	if luasocket then
		--print("socket:Close")
		self.luasocket = nil
		CSocket.close(luasocket)
	end	
end

local function on_new_conn(self,sock)
	self.new_conn:Push({sock})	
	local co = self.block_onaccept:Pop()
	if co then
		Sche.WakeUp(co)--Schedule(co)
	end
end

--[[
在ip:port上建立TCP监听
]]--
function socket:Listen(ip,port)
	if not self.luasocket then
		return "socket close"
	end
	if self.block_onaccept or self.new_conn then
		return "already listening"
	end
	self.block_onaccept = LinkQue.New()
	self.new_conn = LinkQue.New()
	self.__on_new_connection = on_new_conn
	return CSocket.listen(self.luasocket,ip,port)
end

local function process_c_disconnect_event(self,errno)
	print("process_c_disconnect_event")
	self.errno = errno
	local co
	while self.block_noaccept do
		co = self.block_onaccept:Pop()
		if co then
			Sche.WakeUp(co)--Schedule(co)
		else
			self.block_noaccept = nil
		end
	end

	if self.connect_co then
		Sche.WakeUp(self.connect_co)--Schedule(self.connect_co)
	end

	while true do
		co = self.block_recv:Pop()
		if co then
			co = co[1]
			Sche.WakeUp(co)--Schedule(co) 
		else
			break
		end
	end
	
	if self.pending_rpc then
		--唤醒所有等待响应的rpc调用
		for k,v in pairs(self.pending_rpc) do
			print("process pending")
			v.response = {err="remote connection lose",ret=nil}
			Sche.Schedule(v)
		end					
	end
	if self.application then
		self.application.sockets[self] = nil
		self.application = nil
	end
	if self.on_disconnected then
		if Sche.Running() then
			self.on_disconnected(self,errno)
		else
			local s = self
			Sche.Spawn(function ()
					s.on_disconnected(s,errno)
					if s.luasocket then 
						s:Close()
					end
				         end)
			return			
		end
	end
	if self.luasocket then
		self:Close()
	end			
end

local function process_c_packet_event(self,packet)
	self.packet:Push({packet})
	local co = self.block_recv:Pop()
	if co then
	    	self.timeout = nil
	    	co = co[1]
		Sche.WakeUp(co)		
	end
end

function socket:Establish(decoder,recvbuf_size)
	self.isestablish = true
	self.__on_packet = process_c_packet_event
	self.__on_disconnected = process_c_disconnect_event
	self.block_recv = LinkQue.New()	
	if not decoder then
		decoder = CSocket.rawdecoder()
	end
	if not recvbuf_size then
		recvbuf_size = 65535
	end	
	CSocket.establish(self.luasocket,recvbuf_size,decoder)
	self.packet = LinkQue.New()
	return self	
end

--[[
接受一个TCP连接,并将新连接的接收缓冲设为max_packet_size
]]--
function socket:Accept()
	if not self.luasocket then
		return nil,"socket close"
	end

	if not self.block_onaccept or not self.new_conn then
		return nil,"invaild socket"
	else	
		while true do
			local s = self.new_conn:Pop()
			if s then
			    s = s[1]
				local sock = socket:new2(s)
				--establish(sock,max_packet_size or 65535,decoder)
				return sock,nil
			else
				local co = Sche.Running()
				self.block_onaccept:Push(co)
				Sche.Block()
				if  not self.luasocket then
					return nil,"socket close" --socket被关闭
				end				
			end
		end
	end
end

local function cb_connect(self,s,err)
	if not s or err ~= 0 then
		self.errno = err
	else
		self.luasocket = CSocket.new2(self,s)
	end
	local co = self.connect_co
	self.connect_co = nil
	Sche.WakeUp(co)--Schedule(co)	
end

--[[
尝试与ip:port建立TCP连接，如果连接成功建立，将连接的接收缓冲设为max_packet_size
]]--
function socket:Connect(ip,port)
	local ret = CSocket.connect(self.luasocket,ip,port)
	if ret then
		return ret
	else
		self.connect_co = Sche.Running()
		self.___cb_connect = cb_connect
		Sche.Block()
		if not self.luasocket or  self.errno ~= 0 then
			return self.errno
		else
			--establish(self,max_packet_size or 65535,decoder)	
			return nil
		end				
	end
end

--[[
尝试从套接口中接收数据,如果成功返回数据,如果失败返回nil,错误描述
timeout参数如果为nil,则当socket没有数据可被接收时Recv调用将一直阻塞
直到有数据可返回或出现错误.否则在有数据可返回或出现错误之前Recv最少阻塞
timeout毫秒
]]--
function socket:Recv(timeout)
	if not self.isestablish then
		return nil,"invaild socket"
	else
		while true do--not self.closing do	
			local packet = self.packet:Pop()
			if packet then
				return packet[1],nil
			elseif not self.luasocket then
				return nil,self.errno
			end		
			local co = Sche.Running()
			co = {co}	
			self.block_recv:Push(co)		
			self.timeout = timeout
			Sche.Block(timeout)
			self.block_recv:Remove(co) --remove from block_recv
			if self.timeout then
				self.timeout = nil
				return nil,"recv timeout"
			end
		end	
	end
end

--[[
将packet发送给对端，如果成功返回nil,否则返回错误描述.
(此函数不会阻塞,立即返回)
]]--
function socket:Send(packet)
	if not self.luasocket then
		return "socket socket"
	end
	return CSocket.send(self.luasocket,packet)
end

function socket:tostring()
	return CSocket.tostring(self.luasocket)
end

return {
	New = function (domain,type,protocal) return socket:new(domain,type,protocal) end
}

