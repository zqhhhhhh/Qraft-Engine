FollowAIMovement = {

  dirs = {
    { 1,  0},
    {-1,  0},
    { 0,  1},
    { 0, -1},
  },

  distance_field = nil,

  OnStart = function(self)

	self.previous_player_positions = {}

    self.transform = self.actor:GetComponent("Transform")

    local player = Actor.Find("Player")
    if player then
      self.player_transform = player:GetComponent("Transform")
    end
  end,

  IsBlocked = function(self, x, y)
    local k = Round(x) .. "," .. Round(y)
    return blocking_map[k] == true
  end,

  BuildDistanceField = function(self, px, py)

    local dist = {}
    local qx, qy = {}, {}
    local head, tail = 1, 1

    local startKey = px .. "," .. py
    dist[startKey] = 0
    qx[tail], qy[tail] = px, py

    while head <= tail do
      local x, y = qx[head], qy[head]
      head = head + 1
      local base = dist[x .. "," .. y]

      for _, d in ipairs(self.dirs) do
        local nx, ny = x + d[1], y + d[2]
        local k = nx .. "," .. ny

        if dist[k] == nil and not self:IsBlocked(nx, ny) then
          dist[k] = base + 1
          tail = tail + 1
          qx[tail], qy[tail] = nx, ny
        end
      end
    end

    self.distance_field = dist
  end,

  NextStepTowardPlayer = function(self, sx, sy)

    local bestDx, bestDy = 0, 0
    local bestDist = math.huge

    for _, d in ipairs(self.dirs) do
      local nx, ny = sx + d[1], sy + d[2]
      local k = nx .. "," .. ny
      local dval = self.distance_field[k]

      if dval and dval < bestDist then
        bestDist = dval
        bestDx, bestDy = d[1], d[2]
      end

	  -- If two choices are equal, sometimes choose one or the other.
	  if dval and dval == bestDist then
		if math.random() > 0.5 then
			bestDist = dval
        	bestDx, bestDy = d[1], d[2]
		end
	  end
    end

    return bestDx, bestDy
  end,

	Distance = function(p1, p2)
		local dx = p1.x - p2.x
		local dy = p1.y - p2.y
		return math.sqrt(dx*dx + dy*dy)
	end,

  OnTurn = function(self)
    if not gameplay_enabled then return end
    if not self.player_transform then return end

    local px, py = self.player_transform.x, self.player_transform.y

	local distance = self.Distance({x = px, y = py}, {x = self.transform.x, y = self.transform.y})

	-- One tile away : Track player position and detect repeating back-and-forth. If detected, hop directly to player.
	if distance < 1.5 then
		-- Kill player if they re-enter a previous position during a close chase.
		if self.previous_player_positions[px .. "," .. py] then
			self.transform.x = px
			self.transform.y = py
			return
		end

		self.previous_player_positions[px .. "," .. py] = true
	end

  if distance < 1.1 then
    	self.transform.x = px
			self.transform.y = py
			return
  end

    -- Build distance field every turn outward from the player.
	  self:BuildDistanceField(px, py)

    local sx, sy = self.transform.x, self.transform.y
    local dx, dy = self:NextStepTowardPlayer(sx, sy)

    if dx ~= 0 or dy ~= 0 then
      local nx, ny = sx + dx, sy + dy
      if not self:IsBlocked(nx, ny) then
        self.transform.x = nx
        self.transform.y = ny
      end
    end
  end
}