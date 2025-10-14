local Players, HttpService, WebSocketService, CorePackages = game:GetService("Players"), game:GetService("HttpService"), game:GetService("WebSocketService"), game:GetService("CorePackages")
local VirtualInputManager, RobloxReplicatedStorage = Instance.new("VirtualInputManager"), game:GetService("RobloxReplicatedStorage")

local ExecutorContainer, ScriptContainer, ObjectContainer = Instance.new("Folder"), Instance.new("Folder"), Instance.new("Folder")
ExecutorContainer.Name = "Executor"
ExecutorContainer.Parent = RobloxReplicatedStorage

ScriptContainer.Name = "Scripts"
ScriptContainer.Parent = ExecutorContainer

ObjectContainer.Name = "Objects"
ObjectContainer.Parent = ExecutorContainer




local PROCESS_ID, VERSION = %PROCESS_ID%, %VERSION%
local Executor, Bridge, Utils = {}, {}, {}
local user_agent = "Executor/"..VERSION
local client = WebSocketService:CreateClient("ws://localhost:9001")



function Utils:GetRandomModule()
	local children = CorePackages.Packages:GetChildren()
	local module

	while not module or module.ClassName ~= "ModuleScript" do
		module = children[math.random(#children)]
	end

	local clone = module:Clone()
	clone.Name = HttpService:GenerateGUID(false)
	clone.Parent = ScriptContainer

	return clone
end

function Utils:MergeTable(t1, t2)
	for i, v in pairs(t2) do
		t1[i] = v
	end
	return t1
end

function Utils:HttpGet(url, return_raw)
	assert(type(url) == "string", "invalid argument #1 to 'HttpGet' (string expected, got " .. type(url) .. ") ", 2)
	return_raw = return_raw or true
	local response = Executor.request({
		Url = url,
		Method = "GET",
	})

	if return_raw then
		return response.Body
	end

	return HttpService:JSONDecode(response.Body)
end



--// Bridge
Bridge.on_going_requests = {}

function Bridge:Send(data)
	if type(data) == "string" then
		data = HttpService:JSONDecode(data)
	end

	if not data["pid"] then
		data["pid"] = PROCESS_ID
	end

	client:Send(HttpService:JSONEncode(data))

end

function Bridge:SendAndReceive(data, timeout)
	timeout = timeout or 5

	local id = HttpService:GenerateGUID(false)
	data.id = id

	local bindable_event = Instance.new("BindableEvent")
	local response_data

    local connection
    connection = bindable_event.Event:Connect(function(response)
		response_data = response
        connection:Disconnect()
    end)

	self.on_going_requests[id] = bindable_event

	self:Send(data)

	local start_time = tick()
    while not response_data do
        if tick() - start_time > timeout then
            break
        end
		task.wait(0.1)
    end

	self.on_going_requests[id] = nil
    connection:Disconnect()
	bindable_event:Destroy()

	if not response_data then error("timeout waiting for response " .. id, 2) end

    return response_data
end

function Bridge:IsCompilable(source)
	local success, result = pcall(function()
		return self:SendAndReceive({
			["action"] = "is_compilable",
			["source"] = source
		})
	end)

	if not success then 
		return false, result
	end

	if not result["success"] then
		return false, result["message"]
	end

	return true
end

function Bridge:Loadstring(chunk, chunk_name)
	local module = Utils:GetRandomModule()

	local success, result = pcall(function()
		return self:SendAndReceive({
			["action"] = "loadstring",
			["chunk"] = chunk,
			["chunk_name"] = chunk_name,
			["script_name"] = module.Name
		})
	end)

	if not success then 
		return nil, result
	end

	if not result["success"] then
		return nil, result["message"]
	end

	local func = require(module)

	if debug.info(func, "n") ~= chunk_name then
		module:Destroy()
		return nil, "function does not match"
	end

	module.Parent = nil
	return func
end

function Bridge:Request(options)
	local success, result = pcall(function()
		return self:SendAndReceive({
			["action"] = "request",
			["url"] = options.Url,
			["method"] = options.Method,
			["headers"] = options.Headers,
			["body"] = options.Body
		})
	end)

	if not success then 
		error(result, 3)
	end

	if not result["success"] then
		error(result["message"], 3)
	end

	return {
		Success = result["response"]["success"],
		StatusCode = result["response"]["status_code"],
		StatusMessage = result["response"]["status_message"],
		Headers = result["response"]["headers"],
		Body = result["response"]["body"]
	}
end
--\\


--// Executor
function Executor.getgenv()
	return Executor
end

function Executor.request(options)
	assert(type(options) == "table", "invalid argument #1 to 'request' (table expected, got " .. type(options) .. ") ", 2)
	assert(type(options.Url) == "string", "invalid option 'Url' for argument #1 to 'request' (string expected, got " .. type(options.Url) .. ") ", 2)
	options.Method = options.Method or "GET"
	options.Method = options.Method:upper()
	assert(table.find({"GET", "POST", "PUT", "PATCH", "DELETE"}, options.Method), "invalid option 'Method' for argument #1 to 'request' (a valid http method expected, got '" .. options.Method .. "') ", 2)
	assert(not (options.Method == "GET" and options.Body), "invalid option 'Body' for argument #1 to 'request' (current method is GET but option 'Body' was used)", 2)
	if table.find({"POST", "PUT", "PATCH"}, options.Method) then
		assert(options.Body, "invalid option 'Body' for argument #1 to 'request' (current method is " .. options.Method .. " but option 'Body' was not provided)", 2)
	end
	if options.Body then
		assert(type(options.Body) == "string", "invalid option 'Body' for argument #1 to 'request' (string expected, got " .. type(options.Body) .. ") ", 2)
	end
	options.Headers = options.Headers or {}
	if options.Headers then assert(type(options.Headers) == "table", "invalid option 'Headers' for argument #1 to 'request' (table expected, got " .. type(options.Url) .. ") ", 2) end
	options.Headers["User-Agent"] = options.Headers["User-Agent"] or useragent

	local response = Bridge:Request(options)

	return response
end


Executor.game = {}
setmetatable(Executor.game, {
	__index = function(self, index)
		if index == "HttpGet" or index == "HttpGetAsync" then
			return function(self, ...)
				return Utils:HttpGet(...)
			end
		end

		return game[index]
	end,

	__tostring = function(self)
		return game.Name
	end,

	__metatable = getmetatable(game)
})

function Executor.loadstring(chunk, chunk_name)
	assert(type(chunk) == "string", "invalid argument #1 to 'loadstring' (string expected, got " .. type(chunk) .. ") ", 2)
	chunk_name = chunk_name or "loadstring"
	assert(type(chunk_name) == "string", "invalid argument #2 to 'loadstring' (string expected, got " .. type(chunk_name) .. ") ", 2)
	chunk_name = chunk_name:gsub("[^%a_]", "")


	local compile_success, compile_error = Bridge:IsCompilable(chunk)
	if not compile_success then
		return nil, chunk_name .. tostring(compile_error)
	end

	local func, loadstring_error = Bridge:Loadstring(chunk, chunk_name)

	if not func then
		return nil, loadstring_error
	end

	setfenv(func, getfenv(debug.info(2, 'f')))

	return func
end
--\\


client.MessageReceived:Connect(function(data)
	local success, data = pcall(function()
		return HttpService:JSONDecode(data)
	end)
	if not success then return end

	local id = data["id"]
	local _type = data["type"]


	if id and _type == "response" then
		return Bridge.on_going_requests[id]:Fire(data)
	end

	local action = data["action"]
	if not action then return end

	if action == "execute" then
		local response = {}
		response["type"] = "response"
		response["success"] = false

		if id then response["id"] = id end

		if not data["source"] then
			response["message"] = "Missing keys: source"
			return Bridge:Send(response)
		end

		local func, err = Bridge:Loadstring(data["source"], "Executor")

		if not func then
			response["message"] = err
			return Bridge:Send(response)
		end

		setfenv(func, Utils:MergeTable(getfenv(func), Executor))

		task.spawn(func)
		
		print("True")

		response["success"] = true
		return Bridge:Send(response)
	end

end)

client.Closed:Connect(function()
	warn("Websocket server has been closed, execution no longer works")
end)

local is_server_ready = false
client.Opened:Connect(function()
	is_server_ready = true
end)
while not is_server_ready do
	task.wait(0.1)
end

Bridge:Send({
	["action"] = "initialize",
})