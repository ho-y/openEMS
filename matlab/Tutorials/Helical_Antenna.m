%
% Tutorials / helical antenna
%
% Describtion at:
% http://openems.de/index.php/Tutorial:_Helical_Antenna
%
% Tested with
%  - Matlab 2011a / Octave 3.4.3
%  - openEMS v0.0.27
%
% (C) 2012 Thorsten Liebig <thorsten.liebig@uni-due.de>

close all
clear
clc

post_proc_only = 0;

close all

%% setup the simulation
physical_constants;
unit = 1e-3; % all length in mm

f0 = 2.4e9; % center frequency, frequency of interest!
lambda0 = round(c0/f0/unit); % wavelength in mm
fc = 0.5e9; % 20 dB corner frequency

Helix.radius = 20; % --> diameter is ~ lambda/pi
Helix.turns = 10;  % --> expected gain is G ~ 4 * 10 = 40 (16dBi)
Helix.pitch = 30;  % --> pitch is ~ lambda/4
Helix.wire_rad = 1;

gnd.radius = lambda0/2;

% feeding
feed.width = 2;  %feeding port width
feed.heigth = 2;
feed.R = 120;    %feed impedance

% size of the simulation box
SimBox = [1 1 1.5]*2*lambda0;

%% setup FDTD parameter & excitation function
FDTD = InitFDTD( 30000 );
FDTD = SetGaussExcite( FDTD, f0, fc );
BC = {'MUR' 'MUR' 'MUR' 'MUR' 'MUR' 'PML_8'}; % boundary conditions
FDTD = SetBoundaryCond( FDTD, BC );

%% setup CSXCAD geometry & mesh
max_res = floor(c0 / (f0+fc) / unit / 20); % cell size: lambda/20
CSX = InitCSX();

mesh.x = [-SimBox(1)/2-gnd.radius     -Helix.radius:Helix.wire_rad:Helix.radius      SimBox(1)/2+gnd.radius];
mesh.x = SmoothMeshLines( mesh.x, max_res, 1.4); % create a smooth mesh between specified fixed mesh lines

mesh.y = mesh.x;

mesh.z = unique([-SimBox(3)/2    0:Helix.wire_rad:(Helix.turns*Helix.pitch+feed.heigth+Helix.wire_rad)  (feed.heigth+Helix.wire_rad+Helix.turns*Helix.pitch)+SimBox(3)/2 ]);
mesh.z = SmoothMeshLines( mesh.z, max_res, 1.4 );

CSX = DefineRectGrid( CSX, unit, mesh );

%% create helix using the wire primitive
CSX = AddMetal( CSX, 'helix' ); % create a perfect electric conductor (PEC)

ang = linspace(0,2*pi,21);
coil_x = Helix.radius*cos(ang);
coil_y = Helix.radius*sin(ang);
coil_z = ang/2/pi*Helix.pitch;

helix.x=[];
helix.y=[];
helix.z=[];
zpos = feed.heigth+Helix.wire_rad;
for n=0:Helix.turns-1
    helix.x = [helix.x coil_x];
    helix.y = [helix.y coil_y];
    helix.z = [helix.z coil_z+zpos];
    zpos = zpos + Helix.pitch;
end
clear p
p(1,:) = helix.x;
p(2,:) = helix.y;
p(3,:) = helix.z;
CSX = AddWire(CSX, 'helix', 0, p, Helix.wire_rad);
start = [Helix.radius-feed.width/2 -feed.width/2 feed.heigth];
stop  = [Helix.radius+feed.width/2 +feed.width/2 feed.heigth+2*Helix.wire_rad];
CSX = AddBox(CSX,'helix',0,start,stop);

%% create ground (same size as substrate)
CSX = AddMetal( CSX, 'gnd' ); % create a perfect electric conductor (PEC)
start = [0 0 -0.1];
stop  = [0 0  0.1];
CSX = AddCylinder(CSX,'gnd',10,start,stop,gnd.radius);

%% apply the excitation & resist as a current source
start = [Helix.radius-feed.width/2 -feed.width/2 0];
stop  = [Helix.radius+feed.width/2 +feed.width/2 feed.heigth];
[CSX] = AddLumpedPort(CSX, 5 ,1 ,feed.R, start, stop, [0 0 1], 'excite');

%%nf2ff calc
start = [mesh.x(11)      mesh.y(11)     mesh.z(11)];
stop  = [mesh.x(end-10) mesh.y(end-10) mesh.z(end-10)];
[CSX nf2ff] = CreateNF2FFBox(CSX, 'nf2ff', start, stop, 'OptResolution', lambda0/15);

%% prepare simulation folder
Sim_Path = 'tmp_Helical_Ant';
Sim_CSX = 'Helix_Ant.xml';

if (post_proc_only==0)
    [status, message, messageid] = rmdir( Sim_Path, 's' ); % clear previous directory
    [status, message, messageid] = mkdir( Sim_Path );      % create empty simulation folder

    %% write openEMS compatible xml-file
    WriteOpenEMS( [Sim_Path '/' Sim_CSX], FDTD, CSX );

    %% show the structure
    CSXGeomPlot( [Sim_Path '/' Sim_CSX] );

    %% run openEMS
    RunOpenEMS( Sim_Path, Sim_CSX);
end

%% postprocessing & do the plots
freq = linspace( f0-fc, f0+fc, 501 );
U = ReadUI( {'port_ut1','et'}, Sim_Path, freq ); % time domain/freq domain voltage
I = ReadUI( 'port_it1', Sim_Path, freq ); % time domain/freq domain current (half time step is corrected)

% plot feed point impedance
figure
Zin = U.FD{1}.val ./ I.FD{1}.val;
plot( freq/1e6, real(Zin), 'k-', 'Linewidth', 2 );
hold on
grid on
plot( freq/1e6, imag(Zin), 'r--', 'Linewidth', 2 );
title( 'feed point impedance' );
xlabel( 'frequency f / MHz' );
ylabel( 'impedance Z_{in} / Ohm' );
legend( 'real', 'imag' );

% plot reflection coefficient S11
figure
uf_inc = 0.5*(U.FD{1}.val + I.FD{1}.val * feed.R);
if_inc = 0.5*(I.FD{1}.val + U.FD{1}.val / feed.R);
uf_ref = U.FD{1}.val - uf_inc;
if_ref = if_inc - I.FD{1}.val;
s11 = uf_ref ./ uf_inc;
plot( freq/1e6, 20*log10(abs(s11)), 'k-', 'Linewidth', 2 );
grid on
title( 'reflection coefficient S_{11}' );
xlabel( 'frequency f / MHz' );
ylabel( 'reflection coefficient |S_{11}|' );

P_in = 0.5*uf_inc .* conj( if_inc ); % accepted antenna feed power

drawnow

%% NFFF contour plots %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%find resonance frequncy from s11
f_res = f0;

% get accepted antenna power at frequency f0
P_in_0 = interp1(freq, P_in, f0);

% calculate the far field at phi=0 degrees and at phi=90 degrees
thetaRange = unique([0:0.5:90 90:180]);
phiRange = (0:2:360) - 180;
disp( 'calculating far field at phi=[0 90] deg...' );

nf2ff = CalcNF2FF(nf2ff, Sim_Path, f_res, thetaRange*pi/180, phiRange*pi/180,'Mode',1,'Outfile','3D_Pattern.h5','Verbose',1);

theta_HPBW = thetaRange(find(nf2ff.E_norm{1}(:,1)<max(nf2ff.E_norm{1}(:,1))/2,1))*2;

% display power and directivity
disp( ['radiated power: Prad = ' num2str(nf2ff.Prad) ' Watt']);
disp( ['directivity: Dmax = ' num2str(nf2ff.Dmax) ' (' num2str(10*log10(nf2ff.Dmax)) ' dBi)'] );
disp( ['efficiency: nu_rad = ' num2str(100*nf2ff.Prad./real(P_in_0)) ' %']);
disp( ['theta_HPBW = ' num2str(theta_HPBW) ' °']);


%%
E_far_normalized = nf2ff.E_norm{1} / max(nf2ff.E_norm{1}(:)) * nf2ff.Dmax;
DumpFF2VTK([Sim_Path '/3D_Pattern.vtk'],E_far_normalized,thetaRange,phiRange,1e-3);

