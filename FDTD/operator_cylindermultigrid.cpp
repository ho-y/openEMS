/*
*	Copyright (C) 2010 Thorsten Liebig (Thorsten.Liebig@gmx.de)
*
*	This program is free software: you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation, either version 3 of the License, or
*	(at your option) any later version.
*
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY{} without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License
*	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "operator_cylindermultigrid.h"
#include "engine_cylindermultigrid.h"
#include "extensions/operator_ext_cylinder.h"
#include "tools/useful.h"

Operator_CylinderMultiGrid::Operator_CylinderMultiGrid(vector<double> Split_Radii) : Operator_Cylinder()
{
	m_Split_Radii = Split_Radii;
	m_Split_Rad = m_Split_Radii.back();
	m_Split_Radii.pop_back();
}

Operator_CylinderMultiGrid::~Operator_CylinderMultiGrid()
{
	Delete();
}

Operator_CylinderMultiGrid* Operator_CylinderMultiGrid::New(vector<double> Split_Radii, unsigned int numThreads)
{
	if ((Split_Radii.size()==0) || (Split_Radii.size()>CYLIDINDERMULTIGRID_LIMIT))
	{
		cerr << "Operator_CylinderMultiGrid::New: Warning: Number of multigrids invalid! Split-Number: " << Split_Radii.size() << endl;
		return NULL;
	}
	cout << "Create cylindrical multi grid FDTD operator " << endl;
	Operator_CylinderMultiGrid* op = new Operator_CylinderMultiGrid(Split_Radii);
	op->setNumThreads(numThreads);
	op->Init();

	return op;
}

Engine* Operator_CylinderMultiGrid::CreateEngine() const
{
	Engine_CylinderMultiGrid* eng = Engine_CylinderMultiGrid::New(this,m_numThreads);
	return eng;
}

double Operator_CylinderMultiGrid::GetNumberCells() const
{
	if (numLines)
		return (numLines[0]-m_Split_Pos)*(numLines[1])*(numLines[2]) + m_InnerOp->GetNumberCells();
	return 0;
}

bool Operator_CylinderMultiGrid::SetupCSXGrid(CSRectGrid* grid)
{
	if (Operator_Cylinder::SetupCSXGrid(grid)==false)
		return false;

	// make this multigrid use the larger timestep by method 3, since no r==0 singularity can be part of this engine
	m_TimeStepVar = 3;

	if ((numLines[1]-CC_closedAlpha)%2 != 1)
	{
		cerr << "Operator_CylinderMultiGrid::SetupCSXGrid: Error, number of line in alpha direction must be odd... found: " << numLines[1] << endl;
		exit(0);
	}

	//check if mesh is homogenous in alpha-direction
	double diff=discLines[1][1]-discLines[1][0];
	for (unsigned int n=2; n<numLines[1]; ++n)
	{
		if ( fabs((discLines[1][n]-discLines[1][n-1]) - diff)/diff > 1e-10)
		{
			cerr << "Operator_CylinderMultiGrid::SetupCSXGrid: Error, mesh has to be homogenous in alpha direction for multi grid engine, violation found at: " << n << endl;
			exit(0);
		}
	}

	m_Split_Pos = 0;
	for (unsigned int n=0; n<numLines[0]; ++n)
	{
		if (m_Split_Rad < discLines[0][n])
		{
			m_Split_Pos = n;
			if (g_settings.GetVerboseLevel()>0)
				cout << "Operator_CylinderMultiGrid::SetupCSXGrid: Found mesh split position @" << m_Split_Pos << endl;
			m_Split_Rad = discLines[0][n];
			break;
		}
	}
	if ((m_Split_Pos<4) || (m_Split_Pos>numLines[0]-4))
	{
		cerr << "Operator_CylinderMultiGrid::SetupCSXGrid: Error, split invalid..." << endl;
		return false;
	}
	return true;
}

bool Operator_CylinderMultiGrid::SetGeometryCSX(ContinuousStructure* geo)
{
	if (Operator_Cylinder::SetGeometryCSX(geo)==false)
		return false;

	CSRectGrid* grid = geo->GetGrid();

	grid->ClearLines(0);
	grid->ClearLines(1);
	for (unsigned int n=0; n<m_Split_Pos ; ++n)
		grid->AddDiscLine(0,discLines[0][n]);
	for (unsigned int n=0; n<numLines[1]; n+=2)
		grid->AddDiscLine(1,discLines[1][n]);

	if (m_InnerOp->SetGeometryCSX(CSX)==false)
		return false;

	//restore grid to original mesh
	grid->ClearLines(0);
	grid->ClearLines(1);
	for (unsigned int n=0; n<numLines[0]; ++n)
		grid->AddDiscLine(0,discLines[0][n]);
	for (unsigned int n=0; n<numLines[1]; ++n)
		grid->AddDiscLine(1,discLines[1][n]);

	return true;
}

void Operator_CylinderMultiGrid::Init()
{
	Operator_Cylinder::Init();

	if (m_Split_Radii.empty())
		m_InnerOp = Operator_Cylinder::New(m_numThreads);
	else
		m_InnerOp = Operator_CylinderMultiGrid::New(m_Split_Radii,m_numThreads);
}

#ifdef MPI_SUPPORT
void Operator_CylinderMultiGrid::SetTag(int tag)
{
	m_MyTag = tag;
	m_InnerOp->SetTag(tag+1);
}

void Operator_CylinderMultiGrid::SetNeighborUp(int ny, int id)
{
	if (ny==0)
	{
		cerr << "Operator_CylinderMultiGrid::SetNeighborUp: Error: MPI segregation in radial direction not supported for a cylindircal multigrid. Exit!";
		MPI_Barrier(MPI_COMM_WORLD);
		exit(-1);
	}
	Operator_Cylinder::SetNeighborUp(ny,id);
	m_InnerOp->SetNeighborUp(ny,id);
}

void Operator_CylinderMultiGrid::SetNeighborDown(int ny, int id)
{
	if (ny==0)
	{
		cerr << "Operator_CylinderMultiGrid::SetNeighborDown: Error: MPI segregation in radial direction not supported for a cylindircal multigrid. Exit!";
		MPI_Barrier(MPI_COMM_WORLD);
		exit(-1);
	}
	Operator_Cylinder::SetNeighborDown(ny,id);
	m_InnerOp->SetNeighborDown(ny,id);
}
#endif

void Operator_CylinderMultiGrid::CalcStartStopLines(unsigned int &numThreads, vector<unsigned int> &start, vector<unsigned int> &stop) const
{
	vector<unsigned int> jpt = AssignJobs2Threads(numLines[0]- m_Split_Pos + 1, numThreads, true);

	numThreads = jpt.size();

	start.resize(numThreads);
	stop.resize(numThreads);

	start.at(0)= m_Split_Pos-1;
	stop.at(0)= jpt.at(0)-1  + m_Split_Pos-1;

	for (unsigned int n=1; n<numThreads; n++)
	{
		start.at(n) = stop.at(n-1)+1;
		stop.at(n) = start.at(n) + jpt.at(n) - 1;
	}
}


void Operator_CylinderMultiGrid::FillMissingDataStorage()
{
	unsigned int pos[3];

	double EffMat[4];

	for (int ny=0; ny<3; ++ny)
	{
		for (pos[0]=0; pos[0]<m_Split_Pos-1; ++pos[0])
		{
			for (pos[1]=0; pos[1]<numLines[1]; ++pos[1])
			{
				for (pos[2]=0; pos[2]<numLines[2]; ++pos[2])
				{
					Calc_EffMatPos(ny,pos,EffMat);

					if (m_epsR)
						m_epsR[ny][pos[0]][pos[1]][pos[2]] =  EffMat[0];
					if (m_kappa)
						m_kappa[ny][pos[0]][pos[1]][pos[2]] =  EffMat[1];
					if (m_mueR)
						m_mueR[ny][pos[0]][pos[1]][pos[2]] =  EffMat[2];
					if (m_sigma)
						m_sigma[ny][pos[0]][pos[1]][pos[2]] =  EffMat[3];
				}
			}
		}
	}
}

int Operator_CylinderMultiGrid::CalcECOperator( DebugFlags debugFlags )
{
	int retCode=0;
	if (dT)
		m_InnerOp->SetTimestep(dT);

	//calc inner child first
	m_InnerOp->CalcECOperator();

	dT = m_InnerOp->GetTimestep();

	retCode = Operator_Cylinder::CalcECOperator( debugFlags );
	if (GetTimestepValid()==false)
	{
		cerr << "Operator_CylinderMultiGrid::CalcECOperator(): Warning, timestep invalid... resetting..." << endl;
		dT = opt_dT;
		m_InnerOp->SetTimestep(dT);
		m_InnerOp->CalcECOperator();
		retCode = Operator_Cylinder::CalcECOperator( debugFlags );
	}

	//the data storage will only be filled up to m_Split_Pos-1, fill the remaining area here...
	FillMissingDataStorage();
	return retCode;
}

bool Operator_CylinderMultiGrid::SetupExcitation(TiXmlElement* Excite, unsigned int maxTS)
{
	if (!m_InnerOp->SetupExcitation(Excite,maxTS))
		return false;
	return Exc->setupExcitation(Excite,maxTS);
}

void Operator_CylinderMultiGrid::Delete()
{
	delete m_InnerOp;
	m_InnerOp=0;
}

void Operator_CylinderMultiGrid::Reset()
{
	Delete();
	Operator_Cylinder::Reset();
}

void Operator_CylinderMultiGrid::SetBoundaryCondition(int* BCs)
{
	Operator_Cylinder::SetBoundaryCondition(BCs);
	int oldBC = BCs[1];
	BCs[1] = 0; //always PEC in +r-direction
	m_InnerOp->SetBoundaryCondition(BCs);
	BCs[1] = oldBC;
}

void Operator_CylinderMultiGrid::AddExtension(Operator_Extension* op_ext)
{
	//check whether extension is save to use in multi-grid
	if (op_ext->IsCylindricalMultiGridSave(false)==false)
	{
		cerr << "Operator_CylinderMultiGrid::AddExtension: Warning: Operator extension \"" << op_ext->GetExtensionName() << "\" is not compatible with cylindrical multi-grids!! skipping...!" << endl;
		return;
	}

	Operator_Cylinder::AddExtension(op_ext);

	// cylinder extension does not need to be cloned, it will be created by each operator of its own...
	if (dynamic_cast<Operator_Ext_Cylinder*>(op_ext))
		return;

	//check whether extension is save to use in child multi-grid
	if (op_ext->IsCylindricalMultiGridSave(true))
	{
		Operator_Extension* child_Ext = op_ext->Clone(m_InnerOp);
		if (child_Ext==NULL)
		{
			cerr << "Operator_CylinderMultiGrid::AddExtension: Warning, extension: " << op_ext->GetExtensionName() << " can not be cloned for the child operator. Skipping Extension... " << endl;
			return;
		}
		//give the copy to child
		m_InnerOp->AddExtension(child_Ext);
	}
}

void Operator_CylinderMultiGrid::ShowStat() const
{
	m_InnerOp->ShowStat();
	m_InnerOp->ShowExtStat();
	Operator_Cylinder::ShowStat();
}
