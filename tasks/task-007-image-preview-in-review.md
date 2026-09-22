# Task 007: image preview in the change review panel

## Goal

Expanding a changed image in the review panel shows the image, not the line
`Binary files a/logo.png and b/logo.png differ`, which is what `git diff` emits
and what the panel faithfully prints today. A changed image shows its old and
new version side by side, so the change is visible the way a text diff is.

## Which Qt element

`QLabel` with a `QPixmap`. It is the element Qt has for exactly this: it scales
with `QPixmap::scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation)`,
it is a plain widget so it drops into the `FileCard` layout where `DiffBody`
sits now, and it costs nothing at rest. `QGraphicsView` would add pan and zoom
machinery the panel does not need, and painting the image in a custom
`paintEvent` would mean re-implementing the scaling and the alignment `QLabel`
already has. `ImageRenderer` (`src/imagerenderer.{h,cpp}`) is not the right
reuse either: it draws images at grid cell positions for the future
`vim.ui.img` support, which is a different problem.

Decoding goes through `QImageReader`, not `QPixmap::load`, for three reasons:
it reports the image's real size before decoding (`size()`), it can decode from
a `QBuffer` over bytes git handed us rather than from a file, and
`setScaledSize()` lets a large JPEG be decoded straight to display size instead
of being decoded full size and then shrunk.

## What is shown

A new `ImagePreview` widget, used by `FileCard` in place of `DiffBody` when the
file is an image:

- Two panes side by side in a `QHBoxLayout`: the old version left, the new
  version right, each a `QLabel` inside a small column with a caption.
- The caption of each pane reads `<width>×<height> · <size>` (for example
  `512×512 · 41.2 kB`), in the panel's dim color, so a change that is only a
  re-compression is still legible.
- An added file has no left pane, a deleted file no right one; the missing side
  says `added` / `deleted` rather than being an empty frame.
- Both panes scale to the same height so the two images are comparable, and
  neither is ever scaled up: a 16×16 favicon stays 16×16 rather than being
  blown up into a blurry square.
- Transparency is drawn over a checkerboard, painted by the pane, so a
  transparent PNG does not read as a white or black rectangle depending on the
  scheme.
- Below the panes, one line saying what changed dimensionally when it did
  (`512×512 → 256×256`).

The row header keeps its normal shape (name, tag, counts); a changed image has
no meaningful `+`/`-` counts, so the counts are replaced by the file size
delta.

In a narrow panel the two panes would each be uselessly small. Below a
threshold width the layout falls back to stacking them vertically, which the
existing `resizeEvent` on the card is the natural place to decide.

## Where the two versions come from

| Case | Old version | New version |
|---|---|---|
| Modified, tracked | `git show <base>:<path>` | the file on disk |
| Added / untracked | none | the file on disk |
| Deleted | `git show <base>:<path>` | none |

`<base>` is whatever the panel is comparing against, so this follows task 006's
modes without extra work: `HEAD` in unstaged mode, the merge base in branch
mode.

`git show` returns bytes, and the panel's `runGit()` decodes its output into a
`QString`, which would corrupt them. The task needs a binary sibling that hands
back the raw `QByteArray` (same process plumbing, same timeout, same one-run
guard), and `QProcess::setProcessChannelMode` must not be merging stderr into
stdout for that call.

Fetching happens when the row is expanded, not on every poll: an image the user
never opens costs nothing. The fetched bytes are cached on the card and dropped
when the file's diff changes, which the reconcile pass in `apply()` already
detects.

## Recognizing an image

Two conditions, both required:

1. The path's suffix is in `QImageReader::supportedImageFormats()`, lowercased.
   This is the build's real capability rather than a hardcoded list: PNG, JPEG,
   GIF, BMP, WEBP and ICO are in a normal Qt 6 build.
2. The file is binary as far as git is concerned, or untracked. `git diff`
   marks it with `Binary files ... differ`, which `parseDiff()` currently keeps
   as an ordinary body line; that line becomes the flag (`FileDiff::binary`)
   instead of body text.

SVG is deliberately out. Rendering it needs `Qt6::Svg`, which the project has
avoided so far (the app icon is rasterized at build time precisely to keep that
dependency out, see `cmake/AppIcons.cmake`), and an SVG diff is readable as
text anyway, which is what the panel already does well.

A file that passes both checks but fails to decode (a truncated PNG, a format
the build has no plugin for) falls back to the text body it shows today, with
the reason in the dim color. The preview is an enhancement over a working view,
never a replacement for it.

## Configuration

| Key | Default | Meaning |
|---|---|---|
| `image_preview` | true | Show changed images as images; false keeps the `Binary files ... differ` line |
| `image_max_bytes` | 10485760 (10 MiB) | Larger images are not decoded; the row says so and stays textual |

The height cap is the existing `file_max_height`: an image pane obeys the same
limit an expanded diff does, so one huge PNG cannot push every other row off
the panel. Both keys ship commented out in `neobeam.toml.example` and join the
`[review]` key list in `src/config.cpp`.

`image_max_bytes` guards the decode, not the download: git has already produced
the bytes by then. It exists because decoding a 100 MP image would stall the UI
thread for seconds, and `QImageReader::size()` gives the dimensions to refuse
it before that happens.

## Wiring

- `FileDiff` gains `binary`, the old/new byte sizes and (once fetched) the two
  `QByteArray`s.
- `src/changereview.{h,cpp}`: `ImagePreview` (the two panes plus the captions),
  `FileCard` choosing between it and `DiffBody` when the row is expanded.
- `ChangeReviewPanel` gains the binary-safe git runner.
- `parseDiff()` turns the `Binary files` line into the flag.
- README: a paragraph in the review section, and the two config keys.

## Acceptance

1. Changing a tracked PNG and expanding its row shows the old and the new image
   side by side, each captioned with its dimensions and size.
2. A newly added image shows one pane; a deleted one shows the old pane.
3. An untracked image previews from disk with no git call.
4. A transparent PNG reads correctly on a dark and on a light scheme.
5. A 16×16 icon is not scaled up; a 4000×3000 photo is scaled down within
   `file_max_height` and the panel stays responsive while it opens.
6. A file larger than `image_max_bytes`, a corrupt image and an SVG all keep
   the textual row and say why.
7. `image_preview = false` restores today's behaviour exactly.
8. Collapsing and re-expanding a row does not refetch the old version; a poll
   that finds the image changed does.

## Risks

- `git show` on a large blob copies it through a pipe into memory. The size cap
  applies after that copy, so a repository of 100 MB images makes expansion
  expensive even when nothing is decoded. Reading `git cat-file -s` first is
  the escape hatch if that turns out to matter.
- An animated GIF shows its first frame only. Playing it would mean a
  `QMovie` and a timer per open row, which is not worth it for a review panel.
