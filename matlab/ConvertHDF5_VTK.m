function ConvertHDF5_VTK(hdf_file, vtk_prefix, varargin)
% ConvertHDF5_VTK(hdf_file, vtk_prefix, varargin)
%
% Convert openEMS field data stored in the given hdf5 file to a vtk file.
%
% arguments:
%   hdf_file:   source hdf5 file
%   vtk_prefix: output vtk files prefix
%
% optional arguments:
%   'TD_Dump':  activate dump for time-domain data (default is off)
%   'FD_Dump':  activate dump for frequency-domain data (default is on)
%   'NumPhase': number of phase to dump frequency domain data animation
%               (default is 36 --> 10°)
%   'FieldName': field name written to vtk, e.g. 'E-Field'
%
% example:
%   % read time-domian data from hdf5, perform dft and dump as vtk
%   ConvertHDF5_VTK('Et.h5','Ef','NumPhase',18,'Frequency',1e9)
%
% openEMS matlab interface
% -----------------------
% author: Thorsten Liebig
%
% See also ReadHDF5Dump Dump2VTK

do_FD_dump = 1;
do_TD_dump = 0;
phase_N = 36;

fieldname = 'unknown';

for n=1:2:numel(varargin)
    if (strcmp(varargin{n},'TD_Dump')==1);
        do_TD_dump =  varargin{n+1};
    end
    if (strcmp(varargin{n},'FD_Dump')==1);
        do_FD_dump =  varargin{n+1};
    end
    if (strcmp(varargin{n},'NumPhase')==1);
        phase_N =  varargin{n+1};
    end
    if (strcmp(varargin{n},'FieldName')==1);
        fieldname =  varargin{n+1};
    end
end

[field mesh] = ReadHDF5Dump(hdf_file, varargin{:});

if ((do_TD_dump==0) && (do_FD_dump==0))
    warning('openEMS:ConvertHDF5_VTK','FD and TD dump disabled, nothing to be done...');
end

if (do_FD_dump)
    if (~isfield(field,'FD'))
        warning('openEMS:ConvertHDF5_VTK','no FD data found skipping frequency domian vtk dump...');
    else
        ph = linspace(0,360,phase_N+1);
        ph = ph(1:end-1);
        for n = 1:numel(field.FD.freq)
            for p = ph
                filename = [vtk_prefix '_' num2str(field.FD.freq(n)) '_' num2str(p,'%03d') '.vtk' ];
                Dump2VTK(filename, real(field.FD.values{n}*exp(1j*p*pi/180)), mesh, fieldname, varargin{:});
            end
            filename = [vtk_prefix '_' num2str(field.FD.freq(n)) '_abs.vtk' ];
            Dump2VTK(filename, abs(field.FD.values{n}), mesh, fieldname, varargin{:});
            filename = [vtk_prefix '_' num2str(field.FD.freq(n)) '_ang.vtk' ];
            Dump2VTK(filename, angle(field.FD.values{n}), mesh, fieldname, varargin{:});
        end
    end
end

if (do_TD_dump)
    if (~isfield(field,'TD'))
        warning('openEMS:ConvertHDF5_VTK','no TD data found skipping time domian vtk dump...');
    else
        disp('dumping time domain data...')
        acc = ['%0' int2str(ceil(log10(numel(field.TD.time)+1))) 'd'];
        for n = 1:numel(field.TD.time)
            filename = [vtk_prefix '_TD_' num2str(n,acc) '.vtk' ];
            Dump2VTK(filename, field.TD.values{n}, mesh, fieldname, varargin{:});
        end
    end
end
