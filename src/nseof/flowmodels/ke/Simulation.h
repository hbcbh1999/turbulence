#ifndef _NSEOF_FLOWMODELS_KE_SIMULATION_H_
#define _NSEOF_FLOWMODELS_KE_SIMULATION_H_

#include <petscksp.h>
#include <float.h>

#include "../../Definitions.h"
#include "../../FlowFieldSimulation.h"
#include "../../Iterators.h"
#include "../../stencils/RHSStencil.h"
#include "../../stencils/VelocityStencil.h"
#include "../../stencils/ObstacleStencil.h"
#include "../../stencils/MaxUStencil.h"
#include "../../stencils/PeriodicBoundaryStencils.h"
#include "../../stencils/BFStepInitStencil.h"
#include "../../stencils/NeumannBoundaryStencils.h"
#include "../../stencils/BFInputStencils.h"
#include "../../stencils/InitTaylorGreenFlowFieldStencil.h"
#include "../../solvers/PetscSolver.h"
#include "../../parallelManagers/MPICommunicator.h"

#include "../turbulent/FlowField.h"
#include "../turbulent/HStencil.h"
#include "../turbulent/FGHStencil.h"

#include "FlowField.h"
#include "GlobalBoundaryFactory.h"
#include "InitStencil.h"
#include "KEStencilF.h"
#include "KEStencil.h"
#include "NutStencil.h"
#include "ObstacleStencil.h"
#include "PredicateStencil.h"
#include "RHSStencil.h"

namespace nseof {

namespace flowmodels {

namespace ke {

class Simulation : public FlowFieldSimulation<FlowField> {
 protected:
  MaxUStencil _maxUStencil;
  FieldIterator<nseof::FlowField> _maxUFieldIterator;
  GlobalBoundaryIterator<nseof::FlowField> _maxUBoundaryIterator;

  // Set up the boundary conditions
  GlobalBoundaryFactory _globalBoundaryFactory;
  GlobalBoundaryIterator<nseof::FlowField> _wallVelocityIterator;
  GlobalBoundaryIterator<nseof::FlowField> _wallFGHIterator;
  GlobalBoundaryIterator<FlowField> _wallKEIterator;

  nseof::flowmodels::turbulent::FGHStencil _fghStencil;
  FieldIterator<nseof::flowmodels::turbulent::FlowField> _fghIterator;

  nseof::RHSStencil _rhsStencil;
  FieldIterator<nseof::FlowField> _rhsIterator;

  VelocityStencil _velocityStencil;
  nseof::ObstacleStencil _obstacleStencil;
  ObstacleStencil _obstacleStencilKE;
  FieldIterator<nseof::FlowField> _velocityIterator;
  FieldIterator<nseof::FlowField> _obstacleIterator;
  FieldIterator<FlowField> _obstacleIteratorKE;

  std::vector<std::string> _mpiiwvector2D;
  std::vector<std::string> _mpiiwvector3D;
  MPIIteratorWrite<FlowField, double> _mpiiw;
  MPIIteratorRead<FlowField, double> _mpiir;

  NutStencil _nutst;
  FieldIterator<FlowField> _nutit;
  nseof::flowmodels::turbulent::HStencil _hst;
  FieldIterator<nseof::flowmodels::turbulent::FlowField> _hit;

  nseof::flowmodels::turbulent::MaximumNutStencil _maxmnutst;
  FieldIterator<nseof::flowmodels::turbulent::FlowField> _maxmnutit;

  PredicateStencil<FlowField, bool> _tsBool;
  FieldIterator<FlowField> _tiBool;

  KEStencilF _keStencilF;
  FieldIterator<FlowField> _keFit;

  RHSStencil _keStencilRHS;
  FieldIterator<FlowField> _keRHSit;

  KEStencil _keStencil;
  FieldIterator<FlowField> _keit;

  int keloopcounter = 0;

  // clang-format off
  MPICommunicator<std::array<FLOAT, 2>, FlowField> nutComm{
    *this->_flowField, this->_parameters,
    [](FlowField &flowField, int i, int j, int k, std::array<FLOAT, 2> &p) {
      p[0] = flowField.getEpsilon(i, j, k);
      p[1] = flowField.getTke(i, j, k);
    },
    [](FlowField &flowField, int i, int j, int k, std::array<FLOAT, 2> &p) {
      flowField.getEpsilon(i, j, k) = p[0];
      flowField.getTke(i, j, k) = p[1];
    }, 1};
  // clang-format on

 public:
  Simulation(Parameters &parameters, nseof::geometry::GeometryManager &gm)
      : FlowFieldSimulation(parameters, new FlowField(parameters), gm),
        _maxUStencil(parameters),
        _maxUFieldIterator(*_flowField, parameters, _maxUStencil),
        _maxUBoundaryIterator(*_flowField, parameters, _maxUStencil),
        _globalBoundaryFactory(parameters),
        _wallVelocityIterator(
            _globalBoundaryFactory.getGlobalBoundaryVelocityIterator(
                *_flowField)),
        _wallFGHIterator(
            _globalBoundaryFactory.getGlobalBoundaryFGHIterator(*_flowField)),
        _wallKEIterator(
            _globalBoundaryFactory.getGlobalBoundaryIterator(*_flowField)),
        _fghStencil(parameters),
        _fghIterator(*_flowField, parameters, _fghStencil),
        _rhsStencil(parameters),
        _rhsIterator(*_flowField, parameters, _rhsStencil),
        _velocityStencil(parameters),
        _obstacleStencil(parameters),
        _obstacleStencilKE(parameters),
        _velocityIterator(*_flowField, parameters, _velocityStencil),
        _obstacleIterator(*_flowField, parameters, _obstacleStencil),
        _obstacleIteratorKE(*_flowField, parameters, _obstacleStencilKE),
        _mpiiwvector2D{"p",  "u",   "v", "k", "epsilon", "f1",
                       "f2", "fmu", "d", "e", "P"},
        _mpiiwvector3D{"p",  "u",  "v",   "w", "k", "epsilon",
                       "f1", "f2", "fmu", "d", "e", "P"},
        _mpiiw(
            *_flowField, parameters, _mpiiwvector2D, _mpiiwvector3D,
            [](FlowField &flowField, int i, int j, int k, double &p,
               std::vector<int> & table) {
              // clang-format off
              *(&p + table[ 0]) = flowField.getPressure().getScalar(i, j) - flowField.getU(i, j) * flowField.getU(i, j);
              *(&p + table[ 1]) = flowField.getVelocity().getVector(i, j)[0];
              *(&p + table[ 2]) = flowField.getVelocity().getVector(i, j)[1];
              *(&p + table[ 3]) = flowField.getTke(i, j);
              *(&p + table[ 4]) = flowField.getEpsilon(i, j);
              *(&p + table[ 5]) = flowField.getF1(i, j);
              *(&p + table[ 6]) = flowField.getF2(i, j);
              *(&p + table[ 7]) = flowField.getFmu(i, j);
              *(&p + table[ 8]) = flowField.getD(i, j);
              *(&p + table[ 9]) = flowField.getE(i, j);
              *(&p + table[10]) = flowField.getPressure().getScalar(i, j);
              // clang-format on
            },
            [](FlowField &flowField, int i, int j, int k, double &p,
               std::vector<int> & table) {
              // clang-format off
              *(&p + table[ 0]) = flowField.getPressure().getScalar(i, j, k) - flowField.getU(i, j, k) * flowField.getU(i, j, k);
              *(&p + table[ 1]) = flowField.getVelocity().getVector(i, j, k)[0];
              *(&p + table[ 2]) = flowField.getVelocity().getVector(i, j, k)[1];
              *(&p + table[ 3]) = flowField.getVelocity().getVector(i, j, k)[2];
              *(&p + table[ 4]) = flowField.getTke(i, j, k);
              *(&p + table[ 5]) = flowField.getEpsilon(i, j, k);
              *(&p + table[ 6]) = flowField.getF1(i, j, k);
              *(&p + table[ 7]) = flowField.getF2(i, j, k);
              *(&p + table[ 8]) = flowField.getFmu(i, j, k);
              *(&p + table[ 9]) = flowField.getD(i, j, k);
              *(&p + table[10]) = flowField.getE(i, j, k);
              *(&p + table[11]) = flowField.getPressure().getScalar(i, j, k);
              // clang-format on
            }),
        _mpiir(
            *_flowField, parameters, _mpiiwvector2D, _mpiiwvector3D,
            [](FlowField &flowField, int i, int j, int k, double &p,
               std::vector<int> & table) {
              // clang-format off
                 flowField.getPressure().getScalar(i, j)    = table[10] != -1 ? *(&p + table[10]) : (table[0] != -1 ? *(&p + table[0]) : 0.0);
                 flowField.getVelocity().getVector(i, j)[0] = table[1]  != -1 ? *(&p + table[ 1]) : 0.0;
                 flowField.getVelocity().getVector(i, j)[1] = table[2]  != -1 ? *(&p + table[ 2]) : 0.0;
                 flowField.getTke(i, j)                     = table[3]  != -1 ? *(&p + table[ 3]) : 0.0;
                 flowField.getEpsilon(i, j)                 = table[4]  != -1 ? *(&p + table[ 4]) : 0.0;
                 flowField.getF1(i, j)                      = table[5]  != -1 ? *(&p + table[ 5]) : 0.0;
                 flowField.getF2(i, j)                      = table[6]  != -1 ? *(&p + table[ 6]) : 0.0;
                 flowField.getFmu(i, j)                     = table[7]  != -1 ? *(&p + table[ 7]) : 0.0;
                 flowField.getD(i, j)                       = table[8]  != -1 ? *(&p + table[ 8]) : 0.0;
                 flowField.getE(i, j)                       = table[9]  != -1 ? *(&p + table[ 9]) : 0.0;
              // clang-format on
            },
            [](FlowField &flowField, int i, int j, int k, double &p,
               std::vector<int> & table) {
              // clang-format off
                 flowField.getPressure().getScalar(i, j, k)    = table[11] != -1 ? *(&p + table[11]) : (table[0] != -1 ? *(&p + table[0]) : 0.0);
                 flowField.getVelocity().getVector(i, j, k)[0] = table[1]  != -1 ? *(&p + table[ 1]) : 0.0;
                 flowField.getVelocity().getVector(i, j, k)[1] = table[2]  != -1 ? *(&p + table[ 2]) : 0.0;
                 flowField.getVelocity().getVector(i, j, k)[2] = table[3]  != -1 ? *(&p + table[ 3]) : 0.0;
                 flowField.getTke(i, j, k)                     = table[4]  != -1 ? *(&p + table[ 4]) : 0.0;
                 flowField.getEpsilon(i, j, k)                 = table[5]  != -1 ? *(&p + table[ 5]) : 0.0;
                 flowField.getF1(i, j, k)                      = table[6]  != -1 ? *(&p + table[ 6]) : 0.0;
                 flowField.getF2(i, j, k)                      = table[7]  != -1 ? *(&p + table[ 7]) : 0.0;
                 flowField.getFmu(i, j, k)                     = table[8]  != -1 ? *(&p + table[ 8]) : 0.0;
                 flowField.getD(i, j, k)                       = table[9]  != -1 ? *(&p + table[ 9]) : 0.0;
                 flowField.getE(i, j, k)                       = table[10] != -1 ? *(&p + table[10]) : 0.0;
              // clang-format on
            }),
        _nutst(parameters),
        _nutit(*_flowField, _parameters, _nutst, 0, 1),
        _hst(parameters, gm),
        _hit(*_flowField, _parameters, _hst, 0, 1),
        _maxmnutst(parameters),
        _maxmnutit(*_flowField, _parameters, _maxmnutst, 1, 0),
        _tsBool(parameters, false,
                [](bool a, bool b) {
                  if (a) {
                    return true;
                  }
                  if (b) {
                    return false;
                  }

                  return true;
                },
                [](int i, int j, int k, const Parameters &pp, FlowField & ff) {
                  return ((ff.getTke(i, j, k) +
                           pp.timestep.dt * ff.getRHSTke(i, j, k)) <
                          pp.kEpsilon.adapterr) ||
                         ((ff.getEpsilon(i, j, k) +
                           pp.timestep.dt * ff.getRHSEpsilon(i, j, k)) <
                          pp.kEpsilon.adapterr);
                }),
        _tiBool(*_flowField, _parameters, _tsBool, 1, 0),
        _keStencilF(parameters),
        _keFit(*_flowField, _parameters, _keStencilF, 0, 1),
        _keStencilRHS(parameters),
        _keRHSit(*_flowField, _parameters, _keStencilRHS, 0, 1),  // 0, 0
        _keStencil(parameters),
        _keit(*_flowField, _parameters, _keStencil, 0, 1) {  // 0, 0
    // distance to the next wall
    this->scalarStencils.push_back(CellDataStencil<double, FlowField>(
        this->_parameters, "h",
        [](FlowField &f, int i, int j) { return f.getH(i, j); },
        [](FlowField &f, int i, int j, int k) { return f.getH(i, j, k); }));

    // vortex viscosity
    this->scalarStencils.push_back(CellDataStencil<double, FlowField>(
        this->_parameters, "nu",
        [](FlowField &f, int i, int j) { return f.getNu(i, j); },
        [](FlowField &f, int i, int j, int k) { return f.getNu(i, j, k); }));

    // turbulent kinetic energy
    this->scalarStencils.push_back(CellDataStencil<double, FlowField>(
        this->_parameters, "tke",
        [](FlowField &f, int i, int j) { return f.getTke(i, j); },
        [](FlowField &f, int i, int j, int k) { return f.getTke(i, j, k); }));

    // dissipation rate
    this->scalarStencils.push_back(CellDataStencil<double, FlowField>(
        this->_parameters, "epsilon",
        [](FlowField &f, int i, int j) { return f.getEpsilon(i, j); },
        [](FlowField &f, int i, int j, int k) {
          return f.getEpsilon(i, j, k);
        }));

    // f1
    this->scalarStencils.push_back(CellDataStencil<double, FlowField>(
        this->_parameters, "F1",
        [](FlowField &f, int i, int j) { return f.getF1(i, j); },
        [](FlowField &f, int i, int j, int k) { return f.getF1(i, j, k); }));

    // f2
    this->scalarStencils.push_back(CellDataStencil<double, FlowField>(
        this->_parameters, "F2",
        [](FlowField &f, int i, int j) { return f.getF2(i, j); },
        [](FlowField &f, int i, int j, int k) { return f.getF2(i, j, k); }));

    // fmu
    this->scalarStencils.push_back(CellDataStencil<double, FlowField>(
        this->_parameters, "Fmu",
        [](FlowField &f, int i, int j) { return f.getFmu(i, j); },
        [](FlowField &f, int i, int j, int k) { return f.getFmu(i, j, k); }));

    // sijsij
    this->scalarStencils.push_back(CellDataStencil<double, FlowField>(
        this->_parameters, "SijSij",
        [](FlowField &f, int i, int j) { return f.getsijsij(i, j); },
        [](FlowField &f, int i, int j, int k) {
          return f.getsijsij(i, j, k);
        }));

    // mixing length
    this->scalarStencils.push_back(CellDataStencil<double, FlowField>(
        this->_parameters, "lm",
        [](FlowField &f, int i, int j) { return f.getLm(i, j); },
        [](FlowField &f, int i, int j, int k) { return f.getLm(i, j, k); }));

    // velocity fluctuation
    this->scalarStencils.push_back(CellDataStencil<double, FlowField>(
        this->_parameters, "u",
        [](FlowField &f, int i, int j) { return f.getU(i, j); },
        [](FlowField &f, int i, int j, int k) { return f.getU(i, j, k); }));

    // actual pressure (without tke)
    this->scalarStencils.push_back(CellDataStencil<double, FlowField>(
        this->_parameters, "pressureactual",
        [](FlowField &f, int i, int j) {
          return f.getPressure().getScalar(i, j) - f.getU(i, j) * f.getU(i, j);
        },
        [](FlowField &f, int i, int j, int k) {
          return f.getPressure().getScalar(i, j, k) -
                 f.getU(i, j, k) * f.getU(i, j, k);
        }));

    // D
    this->scalarStencils.push_back(CellDataStencil<double, FlowField>(
        this->_parameters, "D",
        [](FlowField &f, int i, int j) { return f.getD(i, j); },
        [](FlowField &f, int i, int j, int k) { return f.getD(i, j, k); }));

    // E
    this->scalarStencils.push_back(CellDataStencil<double, FlowField>(
        this->_parameters, "E",
        [](FlowField &f, int i, int j) { return f.getE(i, j); },
        [](FlowField &f, int i, int j, int k) { return f.getE(i, j, k); }));
  }

  virtual ~Simulation() {}

  /** initialises the flow field according to the scenario */
  virtual void initializeFlowField() {
    if (_parameters.simulation.scenario == "taylor-green") {
      // currently, a particular initialization is only requrid for the
      // taylor-green vortex
      InitTaylorGreenFlowFieldStencil stencil(_parameters);
      FieldIterator<nseof::FlowField> iterator(*_flowField, _parameters,
                                               stencil);
      iterator.iterate();
    } else if (_parameters.simulation.scenario == "channel") {
      BFStepInitStencil stencil(_parameters);
      FieldIterator<nseof::FlowField> iterator(*_flowField, _parameters,
                                               stencil, 0, 1);
      iterator.iterate();
      _wallVelocityIterator.iterate();
    } else if (_parameters.simulation.scenario == "pressure-channel") {
      // set pressure boundaries here for left wall
      const FLOAT value = _parameters.walls.scalarLeft;
      ScalarField &rhs = _flowField->getRHS();

      if (_parameters.geometry.dim == 2) {
        const int sizey = _flowField->getNy();
        for (int i = 0; i < sizey + 3; i++) {
          rhs.getScalar(0, i) = value;
        }
      } else {
        const int sizey = _flowField->getNy();
        const int sizez = _flowField->getNz();
        for (int i = 0; i < sizey + 3; i++)
          for (int j = 0; j < sizez + 3; j++) rhs.getScalar(0, i, j) = value;
      }

      // do same procedure for domain flagging as for regular channel
      BFStepInitStencil stencil(_parameters);
      FieldIterator<nseof::FlowField> iterator(*_flowField, _parameters,
                                               stencil, 0, 1);
      iterator.iterate();
    }
    _solver.reInitMatrix();

    _hit.iterate();

    InitStencil _keinitst(_parameters);
    FieldIterator<FlowField> _keinitit(*_flowField, _parameters, _keinitst, 0,
                                       0);
    _keinitit.iterate();
    _wallKEIterator.iterate();
    _nutit.iterate();
    _keFit.iterate();
  }

  virtual void solveTimestep() {
    FLOAT olddt = _parameters.timestep.dt;
    
    // determine and set max. timestep which is allowed in this simulation
    setTimeStep();
    int icounter = 0;

    // calculate laminar or turbulent?
    if (_parameters.timestep.time >= _parameters.kEpsilon.start) {
      // calculate rhs of epsilon- & tke-transport-equv. and check

      // calculate rhs of epsilon and tke
      _keRHSit.iterate();

      // determin new timestep
      int tempGlobal;
      _parameters.timestep.dt *= 2.0;
      do {
        _tsBool.reset();
        _parameters.timestep.dt /= 2.0;
        _tiBool.iterate();

        int tempLocal = (_tsBool.getValue() &&
                          ((icounter++) < _parameters.kEpsilon.adaptnrs));
	
        MPI_Allreduce(&tempLocal, &tempGlobal, 1, MPI_INT, MPI_MAX,
                      PETSC_COMM_WORLD);
      } while (tempGlobal);

      
      // prevent too high dt jumps
      _parameters.timestep.dt = _parameters.timestep.dt/olddt > 1.1 ?
        olddt*1.1 : _parameters.timestep.dt;
      
      keloopcounter += icounter;

      std::cout << "ke-loop: " << icounter << " of " << keloopcounter << std::endl;

      // update epsilon and tke
      _keit.iterate();
      // correct values on bc
      _wallKEIterator.iterate();
      // set obstacle boundary conditions for tke and epsilon
      _obstacleIteratorKE.iterate();
      // communicate epsilon and tke
      nutComm.communicate(*this->_flowField);
    }

    // compute fgh (turbulent)
    _fghIterator.iterate();
    // set global boundary values
    _wallFGHIterator.iterate();
    // compute the right hand side
    _rhsIterator.iterate();
    // solve for pressure
    _solver.solve();
    // communicate pressure values
    pressureComm.communicate(*this->_flowField);
    // compute velocity
    _velocityIterator.iterate();
    // set obstacle boundaries
    _obstacleIterator.iterate();
    // Iterate for velocities on the boundary
    _wallVelocityIterator.iterate();
    // communicate velocity values
    velocityComm.communicate(*this->_flowField);

    // compute vortex viscosity
    _nutit.iterate();
    // calc help variables: f_mu, f_1 and f_2
    _keFit.iterate();
  }

  virtual void serialize() { _mpiiw.iterate(); }

  virtual void deserialize() { _mpiir.iterate(); }
  
  virtual void init() {
    // from super
    FlowFieldSimulation::init();

    // compute vortex viscosity
    _nutit.iterate();

  }

 protected:
  /** sets the time step*/
  virtual void setTimeStep() {
    _maxmnutst.reset();
    _maxmnutit.iterate();

    FLOAT localMin, globalMin;
    assertion(_parameters.geometry.dim == 2 || _parameters.geometry.dim == 3);
    FLOAT factor = 1.0 / (_parameters.meshsize->getDxMin() *
                          _parameters.meshsize->getDxMin()) +
                   1.0 / (_parameters.meshsize->getDyMin() *
                          _parameters.meshsize->getDyMin());

    // determine maximum velocity
    _maxUStencil.reset();
    _maxUFieldIterator.iterate();
    _maxUBoundaryIterator.iterate();
    if (_parameters.geometry.dim == 3) {
      factor += 1.0 / (_parameters.meshsize->getDzMin() *
                       _parameters.meshsize->getDzMin());
      _parameters.timestep.dt = 1.0 / _maxUStencil.getMaxValues()[2];
    } else {
      _parameters.timestep.dt = 1.0 / _maxUStencil.getMaxValues()[0];
    }

    // determin minimal timestep on domain with formula:
    // dt = 1/(2 * (nu+nut))/(dx_min^-2+dy_min^-2+dz_min^-2))
    localMin = std::min(_parameters.timestep.dt,
                        std::min(std::min(1 / (1 / _parameters.flow.Re +
                                               _maxmnutst.getMaximum()) /
                                              (2 * factor),
                                          1.0 / _maxUStencil.getMaxValues()[0]),
                                 1.0 / _maxUStencil.getMaxValues()[1]));

    globalMin = MY_FLOAT_MAX;
    MPI_Allreduce(&localMin, &globalMin, 1, MY_MPI_FLOAT, MPI_MIN,
                  PETSC_COMM_WORLD);

    _parameters.timestep.dt = globalMin;
    _parameters.timestep.dt *= _parameters.timestep.tau;
    _parameters.timestep.dt =
        std::min(_parameters.timestep.dt, _parameters.timestep.dtu);
  }
};
}
}
}

#endif
