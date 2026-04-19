Player = {
	iframe_cooldown = 0,

	OnStart = function(self)
		self.spriterenderer = self.actor:GetComponent("SpriteRenderer")
		self.transform = self.actor:GetComponent("Transform")
	end,

	OnUpdate = function(self) 

		-- iframes
		if self.iframe_cooldown > 0 then
			self.iframe_cooldown = self.iframe_cooldown - 1

			-- Damage flash red
			local sin_value_01 = (math.sin(Application.GetFrame() * 0.4) + 1) * 0.5
			self.spriterenderer.g = 255 * sin_value_01
			self.spriterenderer.b = 255 * sin_value_01
			self.spriterenderer.r = 255

		else
			-- Become normal color again
			self.spriterenderer.g = 255
			self.spriterenderer.b = 255
			self.spriterenderer.r = 255
		end

	end,

	dead = false,

	TakeDamage = function(self)

		if gameplay_enabled then
			if self.dead == false then
				self.dead = true
				gameplay_enabled = false
				Audio.Play(1, "player_damage", false)

				local new_player_ko = Actor.Instantiate("PlayerKo")
				local t = new_player_ko:GetComponent("Transform")
				t.x = self.transform.x
				t.y = self.transform.y
				danger = false

				Actor.Destroy(self.actor)
			end
		end
	end
}

