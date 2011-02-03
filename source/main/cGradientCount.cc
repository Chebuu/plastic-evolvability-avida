#include "cGradientCount.h"
#include "cWorld.h"
#include "cStats.h"
#include <cmath>
#include <iostream>

/* cGradientCount is designed to give moving peaks of resources. Peaks are capped with plateaus. The slope of the peaks
is height / distance. Consequently, when height = distance from center of peak, the value at that cell = 1. This was 
designed this way because the organims used for this could only consume resources when the value is >= 1. Thus, height also 
gives radius of 'edible' resources (aka the plateau). If plateaus are >1, you get sloped edges leading up to plateau 
cylinders.
  Spread gives the radius of the entire resource peak to the outside of the sloped edge. Organisms could detect resources 
all along the spread, but only consume that portion on the plateau. Thus, spread - plateau = sense radius (smell) while 
plateau = consumable radius (actual food).
  Peaks move within the boundaries set by min/max x and y. If the edible portion of the peak hits the boundary, the peak 
'bounces' (sign of direction of movement changes).
  Smoothness of the movement is controlled by move_a_scaler which is the A in eq1 in Morrison & DeJong 1999. A-values 
need to be between 1 and 4. Values of 1 to ~3 give smooth movements. Larger values should yield chaotic moves.
  Peaks stop moving when they are first bitten.
  Peak values will only be refreshed when either all edible portions (>=1) are consumed or when the decay timestep (in 
updates) is reached, whichever comes first.
  Once bitten, peaks will not move again until refreshed.
  Peak values are refreshed to match initial height, spread, and plateau, but the placement of the refreshed peak is random
within the min/max x and y area. */
/*cGradientCount cannot access the random number generator at the very first update. Thus, it uses the DefaultContext initially*/

cGradientCount::cGradientCount(cWorld* world, int in_peakx, int in_peaky, double in_height, double in_spread, double in_plateau, int in_decay, 
                               int in_max_x, int in_max_y, int in_min_x, int in_min_y, double in_move_a_scaler, int in_updatecount, 
                               int in_worldx, int in_worldy, int in_geometry, int in_halo, int in_halo_inner_radius, int in_halo_width,
                               int in_halo_anchor_x, int in_halo_anchor_y) : 
                               m_world(world), m_peakx(in_peakx), m_peaky(in_peaky), m_height(in_height), m_spread(in_spread), m_plateau(in_plateau), 
                               m_decay(in_decay), m_max_x(in_max_x), m_max_y(in_max_y), m_min_x(in_min_x), m_min_y(in_min_y), 
                               m_move_a_scaler(in_move_a_scaler), m_halo(in_halo), m_halo_inner_radius(in_halo_inner_radius), m_halo_width(in_halo_width),
                               m_halo_anchor_x(in_halo_anchor_x), m_halo_anchor_y(in_halo_anchor_y)
{
  ResizeClear(in_worldx, in_worldy, in_geometry);
  m_counter = 0;
  move_counter = 1;
  moveYscaler = 0.5;
  movesignx = m_world->GetDefaultContext().GetRandom().GetInt(-1,2);
  if (movesignx == 0) {
    if (m_world->GetDefaultContext().GetRandom().GetUInt(0,2)) {
      movesigny = 1;
    }
    else movesigny = -1;
  }
  else movesigny = m_world->GetDefaultContext().GetRandom().GetInt(-1,2);
  UpdateCount(&m_world->GetDefaultContext());
  m_peakx = in_peakx;
  m_peaky = in_peaky;
  UpdateCount(&m_world->GetDefaultContext());
}

double cGradientCount::Distance(double x1, double y1, double x2, double y2) //calculate linear distance from cone peak
{
  return(sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1)));
}

void cGradientCount::UpdateCount(cAvidaContext* ctx)
{  
  if(ctx == NULL) ctx = &(m_world->GetDefaultContext());
  bool has_edible = false;
  // determine if there is any edible food left in the peak (don't refresh the peak values until decay kicks in if there is edible food left)
  for (int ii = 0; ii < GetX() && !has_edible; ii++) {
    for (int jj = 0; jj < GetY(); jj++) {
      if (Element(jj * GetX() + ii).GetAmount() >= 1) {
        has_edible = true;
        break;
      }
    }
  }

  // once a resource cone has been 'bitten', start the clock that counts down to when the entire peak will be
  // refreshed (carcass rots for only so long before disappearing)
  if (has_edible && GetModified()) m_counter++;
  
  // only update resource values at declared update timesteps if there is resource left in the cone
  if (has_edible && m_counter < m_decay && GetModified()) return; 
  
  if (m_counter == m_decay) {
    if (m_halo != 1) {
      m_peakx = ctx->GetRandom().GetUInt(m_min_x + m_height, m_max_x - m_height + 1);                 
      m_peaky = ctx->GetRandom().GetUInt(m_min_y + m_height, m_max_y - m_height + 1);
    }
    if (m_halo == 1) {
      m_peakx = ctx->GetRandom().GetUInt(m_halo_anchor_x - m_halo_inner_radius - m_halo_width, m_halo_anchor_x + m_halo_inner_radius + m_halo_width + 1);                 
      m_peaky = m_halo_anchor_y + sqrt ((m_halo_inner_radius * m_halo_inner_radius) - ((m_peakx - m_halo_anchor_x) * (m_peakx - m_halo_anchor_x)));
    }
    movesignx = ctx->GetRandom().GetInt(-1,2);  
    if (movesignx == 0) {
      if (ctx->GetRandom().GetUInt(0,2)) {
        movesigny = 1;
      }
      else movesigny = -1;
    }
    else movesigny = ctx->GetRandom().GetInt(-1,2);
    SetModified(false);
  }
  
  // move cones by moving m_peakx & m_peaky, but only if the cone has not been bitten 
  // keep cones inside their bounding boxes; bounce them if they hit the edge
  moveYscaler = m_move_a_scaler * moveYscaler * (1 - moveYscaler); 
  
  //we use movesign to determine direction of peak movement
  //first, to get smooth movements, we only allow either the x or y direction change to be evaluated in a single update
  //second, we then decide the change of direction based on the current direction so that peak can't 'jump' from -1 to 1, 
  //without first changing to 0
  //finally, we only do this only after every 20 updates to slow the frequency of path changes
  int choosesign = ctx->GetRandom().GetInt(0,3);
  
  if (move_counter == 20) {
    move_counter = 1;
    
    if (choosesign == 1) {
      if (movesignx == -1) movesignx = ctx->GetRandom().GetInt(-1,1); 
      else if (movesignx == 1) movesignx = ctx->GetRandom().GetInt(0,2);
      else movesignx = ctx->GetRandom().GetInt(-1,2);
    }
    
    if (choosesign == 2){ 
      if (movesigny == -1) movesigny = ctx->GetRandom().GetInt(-1,1); 
      else if (movesigny == 1) movesigny = ctx->GetRandom().GetInt(0,2);
      else movesigny = ctx->GetRandom().GetInt(-1,2);
    }
  }
  else move_counter = move_counter++;
  
  double temp_peakx = m_peakx + (moveYscaler * movesignx);
  double temp_peaky = m_peaky + (moveYscaler * movesigny);
  
  if (m_halo == 1) {
    if (choosesign == 1) {
      if (temp_peakx > (m_halo_anchor_x + m_halo_inner_radius + m_halo_width - m_height)) movesignx = -1.0;    
      if (temp_peakx < (m_halo_anchor_x - m_halo_inner_radius - m_halo_width + m_height)) movesignx = 1.0; 
      m_peakx = m_peakx + (movesignx * moveYscaler)+.5;       
      int peakx_dist_from_center = m_peakx - m_halo_anchor_x;
      int max_varx = max ((m_halo_inner_radius * m_halo_inner_radius), (peakx_dist_from_center * peakx_dist_from_center));
      int min_varx = min ((m_halo_inner_radius * m_halo_inner_radius), (peakx_dist_from_center * peakx_dist_from_center));
      m_peaky = m_halo_anchor_y + sqrt (max_varx - min_varx);
    }
    
    if (choosesign == 2) {
      if (temp_peaky > (m_halo_anchor_y + m_halo_inner_radius + m_halo_width - m_height)) movesigny = -1.0;
      if (temp_peaky < (m_halo_anchor_y - m_halo_inner_radius - m_halo_width + m_height)) movesigny = 1.0;
      m_peaky = m_peaky + (movesigny * moveYscaler)+.5; 
      int peaky_dist_from_center = m_peaky - m_halo_anchor_y;
      int max_vary = max ((m_halo_inner_radius * m_halo_inner_radius), (peaky_dist_from_center * peaky_dist_from_center));
      int min_vary = min ((m_halo_inner_radius * m_halo_inner_radius), (peaky_dist_from_center * peaky_dist_from_center));
      m_peakx = m_halo_anchor_x + sqrt (max_vary - min_vary);
    }
  }
  else {
    if (temp_peakx > (m_max_x - m_height)) movesignx = -1.0;
    if (temp_peakx < (m_min_x + m_height)) movesignx = 1.0; 
    
    if (temp_peaky > (m_max_y - m_height)) movesigny = -1.0;
    if (temp_peaky < (m_min_y + m_height)) movesigny = 1.0;
    
    m_peakx = m_peakx + (movesignx * moveYscaler)+.5;
    m_peaky = m_peaky + (movesigny * moveYscaler)+.5; 
  }  
  
  double thisdist;
  double thisheight = 0.0;
  
  for (int ii = 0; ii < GetX(); ii++) {
    for (int jj = 0; jj < GetY(); jj++) {
      thisdist = Distance(ii, jj, m_peakx, m_peaky);
      if (m_spread >= thisdist) {
  // determine individual cells values and add one to distance from center (e.g. so that center point = radius 1, not 0)
          thisheight = m_height / (thisdist + 1);       
  // create cylindrical profiles of resources whereever thisheight would be >1 (area where thisdist + 1 <= m_height) and slopes outside of that range
  // plateau = -1 turns off this option; if activated, causes 'peaks' to be flat plateaus = plateau value; you'll get 
        if (thisheight >= 1 && m_plateau >= 0.0) thisheight = m_plateau; 
      }
      else
        thisheight = 0;

      Element(jj*GetX()+ii).SetInitial(thisheight);
      Element(jj*GetX()+ii).SetAmount(thisheight);
    }
  }   
  ResetResourceCounts();
  m_counter = 0;                            //reset decay counter after cone resources updated
}
