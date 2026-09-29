-- ~/.config/nvim/plugin/nvbeam_preview.lua
-- Example Lua code
local group = vim.api.nvim_create_augroup("NvbeamPreview", {})

vim.api.nvim_create_autocmd("CursorMoved", {
  group = group,
  pattern = "*",
  callback = function()
    if vim.bo.filetype ~= "netrw" then return end
    local line = vim.fn.getline(".")
    local file = vim.fn.fnamemodify(line:match("%S+$") or "", ":p")
    local ext = vim.fn.fnamemodify(file, ":e"):lower()
    local image_exts = { png=1, jpg=1, jpeg=1, gif=1, bmp=1, webp=1, svg=1 }

    if image_exts[ext] and vim.fn.filereadable(file) == 1 then
      -- Show preview in the right half of the window
      local win = vim.api.nvim_get_current_win()
      local info = vim.fn.getwininfo(win)[1]
      vim.rpcnotify(0, "nvbeam_image_show", {
        path = file,
        row = info.topline,
        col = math.floor(info.width / 2),
        width = math.floor(info.width / 2),
        height = info.height,
      })
    else
      vim.rpcnotify(0, "nvbeam_image_hide")
    end
  end,
})

