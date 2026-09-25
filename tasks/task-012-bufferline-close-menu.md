# Task 012: close buffers from the bufferline context menu

## Problem

The bufferline at the top of the editor lists every listed buffer and lets the
user switch to one by clicking it, but there is no way to close one from the
GUI. Getting rid of a buffer means typing `:bd` in Neovim, and getting rid of a
group of them (everything but the current file, everything opened after it)
means a hand-written `:bd` range or a plugin. A right-click menu on the tab is
the expected place for that in a VS Code-like client.

## Change

### Menu

`TablineWidget` gets a custom context menu (`Qt::CustomContextMenu`), enabled
only in buffer mode - the tabpage mode of the same widget closes nothing, and
its entries would be meaningless. The menu opens on the tab under the cursor
(`tabAt()`), not on the current tab: right-clicking a background tab acts on
that tab, as in VS Code. A right-click on empty tabbar space opens no menu.

Entries, in order, each acting on the clicked tab's buffer id:

- **Close buffer** - the clicked buffer.
- **Close others** - every listed buffer except the clicked one.
- **Close all to the right** - the buffers after the clicked one, in bufferline
  order.
- **Close all to the left** - the buffers before it.

An entry whose set is empty (Close others with a single buffer open, Close all
to the right on the last tab) is disabled rather than hidden, so the menu keeps
a stable shape. The menu is styled like the terminal panel's one
(`terminalpanel.cpp`) so the two look the same.

The widget only decides which buffers the entry means and emits a single
`closeBuffers(QVector<int>)`; it does no Neovim calls of its own, matching the
existing `bufferSelected` split of responsibilities.

### Closing

`MainWindow` handles `closeBuffers` through a new Lua helper next to
`neobeam.bufferline()` in `lua/bufferline.lua`, called with the id list, so a
group close is one RPC round trip instead of one per buffer:

- Unmodified buffers are deleted with `nvim_buf_delete({force = false})`.
- Modified buffers are never discarded silently. The helper skips them and
  returns their names; the GUI then asks in a modal dialog whether to discard
  the changes in those buffers, listing them by name. On confirmation it calls
  the helper again for that subset with `force = true`; otherwise those buffers
  stay open and the rest are already gone.
- A buffer displayed in a window is handled by Neovim's own `:bd` semantics
  (the window falls back to another buffer); closing the last listed buffer
  leaves the empty `[No Name]` buffer, as `:bd` does.

The bufferline refreshes on the next flush, as it already does after any buffer
change, so no extra repaint path is needed.

## Non-goals

- No close button ("x") on the tabs themselves.
- No middle-click close.
- No menu in tabpage mode (closing tabpages is a separate feature).
- No reordering or pinning of buffers.

## Verification

With `bufferline` enabled and several files open: right-click a background tab
and close it, the current buffer is unchanged. "Close others" on a background
tab leaves that one buffer and switches to it. Modify a file, then close its
group - the dialog names that file, "Keep open" leaves it in the bufferline
with its modified marker, "Discard" removes it.
