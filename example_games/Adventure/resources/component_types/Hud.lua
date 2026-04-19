Hud = {

	OnStart = function(self)
		dialogue_messages = {}
		danger = false
	end,

	OnLateUpdate = function(self)
		-- Draw Score
		--Render.DrawUI(self.sprite, self.transform.x, self.transform.y, 0)
		if Scene.GetCurrent() == "gameplay" then
			Text.Draw("Score : " .. score, 500, 10, "NotoSans-Regular", 20, 255, 255, 255, 255)
		end

		-- Draw dialogue
		for i,dialogue in ipairs(dialogue_messages) do
			Text.Draw(dialogue, 20, 330 - i * 30, "NotoSans-Regular", 20, 255, 255, 255, 255)
		end

		-- Draw shell
		Image.DrawUI("hud_shell", 0, 0)
		-- Draw Danger shell
		--Image.Draw("hud_shell", 0, 0)

		if danger then
			Image.DrawUI("danger_shell", 0, 0)
		end

		dialogue_messages = {}
	end
}

