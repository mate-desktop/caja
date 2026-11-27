private_icons = [
  'hicolor_emblems_16x16_emblem-note.png',
  'hicolor_emblems_24x24_emblem-note.png',
  'hicolor_emblems_48x48_emblem-note.png',
]

foreach icon : private_icons
  parts = icon.split('_')
  theme = parts[0]
  size = parts[1]
  context = parts[2]
  iconfile = parts[3]

  install_data(
    icon,
    install_dir: join_paths(datadir, 'icons', theme, size, context),
  )
endforeach