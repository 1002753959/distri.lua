

local t = {
	1,
	"hello",
	{3,4},
	fuck = "you",
              hello = function () print("hello") end
}
--setmetatable(t,{})

local wpk = CPacket.NewWPacket(512)

wpk:Write_table(t)
local tt = CPacket.NewRPacket(wpk):Read_table()

print(tt[1])
print(tt[2])
print(tt[3][1])
print(tt[3][2])
print(tt.fuck)
print(tt.hello)

--[[
local ab = {
    person = {
        {
            name = "Alice",
            id = 10000,
            phone = {
                {
                    number = "123456789",
                    type = 1,
                },
                {
                    number = "87654321",
                    type = 2,
                },
            }
        },
        {
            name = "Bob",
            id = 20000,
            phone = {
                {
                    number = "01234567890",
                    type = 3,
                }
            }
        }

    }
}


t = os.clock()

for i = 1,1000000 do
	local wpk = CPacket.PackTable(ab)
end

print(os.clock() - t)
]]--