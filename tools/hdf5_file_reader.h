/*
*	Copyright (C) 2011 Thorsten Liebig (Thorsten.Liebig@gmx.de)
*
*	This program is free software: you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation, either version 3 of the License, or
*	(at your option) any later version.
*
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License
*	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef HDF5_FILE_READER_H
#define HDF5_FILE_READER_H

#include <string>
#include <vector>
#include <complex>
#include <hdf5.h>

class HDF5_File_Reader
{
public:
	HDF5_File_Reader(std::string filename);
	virtual ~HDF5_File_Reader();

	bool ReadMesh(float** lines, unsigned int* numLines, int &meshType);

	//! Get the number of timesteps stored at /FieldData/TD/<NUMBER_OF_TS>
	unsigned int GetNumTimeSteps();
	bool ReadTimeSteps(std::vector<unsigned int> &timestep, std::vector<std::string> &names);

	/*!
	  Get time-domain data stored at /FieldData/TD/<NUMBER_OF_TS>
	  \param[in] ids	time step index to extract
	  \param[out] time	time attribute for the given timestep
	  \param[out] data_size data size found
	  \return field data found in given timestep, caller must delete array, returns NULL if timestep was not found
	  */
	float**** GetTDVectorData(size_t idx, float &time, unsigned int data_size[4]);

	unsigned int GetNumFrequencies();
	bool ReadFrequencies(std::vector<float> &frequencies);
	std::complex<float>**** GetFDVectorData(size_t idx, float &frequency, unsigned int data_size[4]);

	/*!
	  Calculate
	  */
	bool CalcFDVectorData(std::vector<float> &frequencies, std::vector<std::complex<float>****> &FD_data, unsigned int data_size[4]);

	bool IsValid();

protected:
	std::string m_filename;

	bool ReadDataSet(std::string ds_name, hsize_t &nDim, hsize_t* &dims, float* &data);
};

#endif // HDF5_FILE_READER_H
