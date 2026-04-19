GameManager = {
	num_enemies = 10,
	base_enemy_movement_speed = -1.5,

	OnStart = function(self)

		-- Establish global function
		ProcessTurn = function(self)

			if gameplay_enabled then
				-- Iterate through all registered actors that have a TurnBasedMovement component on them.
				for i,turn_based_actor in ipairs(turn_based_actors) do
					-- MOVEMENT
					local basic_ai_movement = turn_based_actor:GetComponent("BasicAIMovement")
					if basic_ai_movement ~= nil then
						basic_ai_movement:OnTurn()
					end

					local follow_ai_movement = turn_based_actor:GetComponent("FollowAIMovement")
					if follow_ai_movement ~= nil then
						follow_ai_movement:OnTurn()
					end

					-- DAMAGE
					local kill_on_touch = turn_based_actor:GetComponent("KillOnTouch")
					if kill_on_touch ~= nil then
						kill_on_touch:OnTurn()
					end


					-- VISION
					local cone_vision = turn_based_actor:GetComponent("ConeVision")
					if cone_vision ~= nil then
						cone_vision:OnTurn()
					end


					-- AT END of turn for an actor, please update blocking
					local blocking = turn_based_actor:GetComponent("Block")
					if blocking ~= nil then
						blocking:OnTurn()
					end
				end
			end

		end

		math.randomseed(494)
		self:CreateStage2(next_stage)
		gameplay_enabled = true
	end,

	CreateStage2 = function(self, stage_number)

		blocking_map = {}
		tile_map = {}
		score = 0
		game_over = false

		-- Load Data from file
		local map_file_path = "resources/data/map" .. stage_number .. ".lua"
		local map = dofile(map_file_path)

		for _, layer in ipairs(map.layers) do
			if layer.type == "objectgroup" then
				for _, obj in ipairs(layer.objects) do
					local x = obj.x or 0
					local y = obj.y or 0
					local gid = obj.gid - 1 -- Lua is one-indexed, so we need to subtract 1 to match up with Tiled's ID system

					-- Normalize X and Y positions
					x = x / 100
					y = y / 100

					if gid == 15 then -- floor_tile
						local new_tile = Actor.Instantiate("Tile")
						local new_tile_t = new_tile:GetComponent("Transform")
						new_tile_t.x = x
						new_tile_t.y = y
					end

					if gid == 4 then -- Villy 
						local new_enemy = Actor.Instantiate("Villy")
						local new_enemy_t = new_enemy:GetComponent("Transform")
						new_enemy_t.x = x
						new_enemy_t.y = y
					end

					if gid == 5 then -- block
						local new_box = Actor.Instantiate("Block")
						local new_box_t = new_box:GetComponent("Transform")
						new_box_t.x = x
						new_box_t.y = y
					end

					if gid == 2 then -- player
						local new_player = Actor.Instantiate("Player")
						local new_player_t = new_player:GetComponent("Transform")
						new_player_t.x = x
						new_player_t.y = y
					end
					
					if gid == 6 then -- manager
						local new_npc = Actor.Instantiate("NPC")
						local new_npc_t = new_npc:GetComponent("Transform")
						new_npc_t.x = x
						new_npc_t.y = y
						new_npc:GetComponent("NPC").dialogue = "Spot, We'll guard the crown. Go get help. Head straight north."
						new_npc:GetComponent("SpriteRenderer").sprite = "boss"
					end

					if gid == 8 then -- cuff
						local new_npc = Actor.Instantiate("NPC")
						local new_npc_t = new_npc:GetComponent("Transform")
						new_npc_t.x = x
						new_npc_t.y = y
						new_npc:GetComponent("NPC").dialogue = obj.properties.dialogue
						new_npc:GetComponent("SpriteRenderer").sprite = "cuff"
					end
					
					if gid == 7 then -- crown
						local new_npc = Actor.Instantiate("NPC")
						local new_npc_t = new_npc:GetComponent("Transform")
						new_npc_t.x = x
						new_npc_t.y = y
						new_npc:GetComponent("NPC").dialogue = "(it glimmers with an ancient, royal aura)"
						new_npc:GetComponent("SpriteRenderer").sprite = "crown"
					end

					if gid == 9 then -- doors 
						local new_door = Actor.Instantiate("Door")
						local new_door_t = new_door:GetComponent("Transform")
						new_door_t.x = x
						new_door_t.y = y

						new_door:GetComponent("AdvanceStageOnTouch").stage = obj.properties.stage
					end

					if gid == 10 then -- ninja 
						local new_enemy = Actor.Instantiate("Enemy")
						local new_enemy_t = new_enemy:GetComponent("Transform")
						new_enemy_t.x = x
						new_enemy_t.y = y
						new_enemy:GetComponent("NPC").dialogue = "Show us where the crown is, or pay the price!"
						new_enemy:GetComponent("SpriteRenderer").sprite = "enemy1"

						local movement = new_enemy:GetComponent("BasicAIMovement")
						movement.vel_x = obj.properties.vx
						movement.vel_y = obj.properties.vy
					end

					if gid == 11 then -- infiltrator dude
						local new_enemy = Actor.Instantiate("Enemy")
						local new_enemy_t = new_enemy:GetComponent("Transform")
						new_enemy_t.x = x
						new_enemy_t.y = y
						new_enemy:GetComponent("NPC").dialogue = "Show us where the crown is, or pay the price!"
						new_enemy:GetComponent("SpriteRenderer").sprite = "enemy2"

						local movement = new_enemy:GetComponent("BasicAIMovement")
						movement.vel_x = obj.properties.vx
						movement.vel_y = obj.properties.vy
					end

					if gid == 12 then -- burglar
						local new_enemy = Actor.Instantiate("Enemy")
						local new_enemy_t = new_enemy:GetComponent("Transform")
						new_enemy_t.x = x
						new_enemy_t.y = y
						new_enemy:GetComponent("NPC").dialogue = "Show us where the crown is, or pay the price!"
						new_enemy:GetComponent("SpriteRenderer").sprite = "enemy3"

						local movement = new_enemy:GetComponent("BasicAIMovement")
						movement.vel_x = obj.properties.vx
						movement.vel_y = obj.properties.vy
					end

					if gid == 13 then -- spy lady
						local new_enemy = Actor.Instantiate("Enemy")
						local new_enemy_t = new_enemy:GetComponent("Transform")
						new_enemy_t.x = x
						new_enemy_t.y = y
						new_enemy:GetComponent("NPC").dialogue = "Where do you think you're going, kid?"
						new_enemy:GetComponent("SpriteRenderer").sprite = "enemy4"

						local movement = new_enemy:GetComponent("BasicAIMovement")
						movement.vel_x = obj.properties.vx
						movement.vel_y = obj.properties.vy
					end


				end
			end
		end
	end
}

