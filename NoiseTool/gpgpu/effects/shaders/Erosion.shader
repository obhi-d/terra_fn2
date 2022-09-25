[shader];

Erosion.glsl;

[parameters];

int   Iteration[1, 100] = 1;
int   SeedOffset        = 0;
float RainRate          = 0.0008;
float Evaporation       = 0.0005;
float MinHeightDelta    = 0.05;
float ReposeSlope       = 0.03;
float Gravity           = 30.0;
float GradientSigma     = 0.5;
float SedimentCapacity  = 50.0;
float DissolvingRate    = 0.25;
float DepositingRate    = 0.001;
