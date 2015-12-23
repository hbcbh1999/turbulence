#ifndef _STENCIL_FGH_TURB_H_
#define _STENCIL_FGH_TURB_H_

#include "../FlowField.h"
#include "../Stencil.h"
#include "../Parameters.h"
#include "../FlowFieldTurbA.h"

class FGHStencilTurb : public FieldStencil<FlowFieldTurbA> {
 private:
  // A local velocity variable that will be used to approximate derivatives.
  // Size matches 3D
  // case, but can be used for 2D as well.
  FLOAT _localVelocity[27 * 3];
  // local meshsize
  FLOAT _localMeshsize[27 * 3];
  // pm: 2015.11.07
  // local nu
  FLOAT _localNu[27 * 3];

 public:
  FGHStencilTurb(const Parameters& parameters);

  /** Apply the stencil in 2D
   *
   * Performs the operation of the stencil in a single position given by the
   * indexes.
   * @param flowField State of the flow
   * @param i Index in the x direction
   * @param j Index in the y direction
   */
  void apply(FlowFieldTurbA& flowField, int i, int j);

  /** Apply the stencil in 3D
   *
   * @param flowField State of the flow
   * @param i Index in the x direction
   * @param j Index in the y direction
   * @param k Index in the z direction
   */
  void apply(FlowFieldTurbA& flowField, int i, int j, int k);
};

#endif