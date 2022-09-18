
// clang-format off
// ui properties
PROPERTY( std::string,     name,                 SettingsUI_NewLine,  "unnamed",      0,   0,    "Name used in save files." )
PROPERTY( int32_t,         seed,                 SettingsUI_NewLine,  1337,           0,   0,     "Random seed" )
PROPERTY( float,           frequency,            SettingsUI_NewLine,  0.02,           0,   0,     "Sample frequency" )
PROPERTY( Vector2i,        gridSize,             SettingsUI_NewLine,  Vector2i{126 }, 2,   10000, "Grid size determines size of each grid cell" )
PROPERTY( Vector2i,        gridCount,            SettingsUI_NewLine,  Vector2i{1   }, 2,   40,    "Number of grids to draw in mesh (Peview only)" )
PROPERTY( Vector2i,        offset,               SettingsUI_NewLine,  Vector2i{-64 }, 0,   0,     "Offset the start vertex of mesh (Peview only)" )
PROPERTY( float,           heightMultiplier,     SettingsUI_NewLine,  20.f,           0,   0,     "Height multiplier (Peview only)" )
PROPERTY( int32_t,         renderStyle,          SettingsUI_NewLine,  31,             1,   100,   "Render style resolution (Peview only)" )
PROPERTY( ColorLayerValue, strataColorPerHeight, SettingsUI_NewLine,  {},             0,   0,     "Add color layer to sample from (x,y,z) and alpha is limiting height (Peview only)" )
PROPERTY( Color3,          sunColor,             SettingsUI_SameLine, Color3( 1.0f ), 0,   0,     "Sun color (Peview only)" )
PROPERTY( float,           sunIntensity,         SettingsUI_NewLine,  1.0f,           0,   0,     "Sun intensity (Peview only)" )
PROPERTY( Rotation,        sunRotation,          SettingsUI_NewLine,  {},             0,   0,     "Sun rotation theta and phi (Peview only)" )

// hidden properties
PROPERTY( Vector2i,        exportGridSize,  ExportUI_NewLine,  Vector2i{0},    0, 0, "Grid size (Export only)" )
PROPERTY( Vector2i,        exportGridStart, ExportUI_NewLine,  Vector2i{0},    0, 0, "Grid start (Export only)" )
PROPERTY( Vector2i,        exportGridCount, ExportUI_NewLine,  Vector2i{0},    0, 0, "Grid count (Export only)" )
PROPERTY( Directory,       exportPath,      ExportUI_NewLine,  "",             0, 0, "Export directory" )

// 
PROPERTY( GeneratorPtr,    generator,       Hidden,            {},             0, 0, "" )

// clang-format on