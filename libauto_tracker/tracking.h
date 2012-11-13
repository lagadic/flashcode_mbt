#ifndef __TRACKING_H__
#define __TRACKING_H__
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/p_square_quantile.hpp>

#include <iostream>
// back-end
#include <boost/msm/back/state_machine.hpp>
//front-end
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/array.hpp>
#include <visp/vpImage.h>


#include <visp/vpImage.h>
#include <visp/vpRGBa.h>
#include <visp/vpHomogeneousMatrix.h>
#include <visp/vpCameraParameters.h>

#include <visp/vpDisplay.h>
#include <visp/vpHinkley.h>
#include <visp/vpMe.h>
#include <vector>
#include <fstream>

#include "cmd_line/cmd_line.h"
#include "detectors/detector_base.h"
#include <visp/vpMbEdgeTracker.h>
#include "states.hpp"
#include "events.h"
#include "tracking_events.h"

using namespace boost::accumulators;
namespace msm = boost::msm;
namespace mpl = boost::mpl;
namespace tracking{
  //class EventsBase;
  class Tracker_ : public msm::front::state_machine_def<Tracker_>{
  public:
    typedef struct {
      boost::accumulators::accumulator_set<
        double,
        boost::accumulators::stats<
          boost::accumulators::tag::median(boost::accumulators::with_p_square_quantile),
          boost::accumulators::tag::max,
          boost::accumulators::tag::mean
        >
      > var,var_x,var_y,var_z,var_wx,var_wy,var_wz,checkpoints;

    } statistics_t;
  private:
    CmdLine cmd;
    int iter_;
    std::ofstream varfile_;
    detectors::DetectorBase* detector_;
    vpMbTracker* tracker_; // Create a model based tracker.

    typedef boost::array<vpHinkley,6> hinkley_array_t;
    hinkley_array_t hink_;


    vpMe tracker_me_config_;
    vpImage<vpRGBa> *I_;
    vpImage<vpRGBa> *_I;
    vpHomogeneousMatrix cMo_; // Pose computed using the tracker.
    vpCameraParameters cam_;
    vpImage<unsigned char> Igray_;
    std::vector<vpPoint> outer_points_3D_bcp_;
    std::vector<vpPoint> points3D_inner_;
    std::vector<vpPoint> points3D_outer_;
    std::vector<vpPoint> points3D_middle_;
    std::vector<vpPoint> f_;
    vpRect vpTrackingBox_;
    cv::Rect cvTrackingBox_;


    statistics_t statistics;
    vpImagePoint flashcode_center_;

    bool flush_display_;

    EventsBase& tracking_events_;
  public:
    //getters to access useful members
    void set_flush_display(bool val);
    bool get_flush_display();
    EventsBase& get_tracking_events();
    detectors::DetectorBase& get_detector();
    vpMbTracker& get_mbt();
    std::vector<vpPoint>& get_points3D_inner();
    std::vector<vpPoint>& get_points3D_outer();
    std::vector<vpPoint>& get_points3D_middle();
    std::vector<vpPoint>& get_flashcode();
    template<class T>
    const T& get_tracking_box();
    vpImage<vpRGBa>& get_I();
    vpCameraParameters& get_cam();
    CmdLine& get_cmd();

    Tracker_(CmdLine& cmd, detectors::DetectorBase* detector,vpMbTracker* tracker_,EventsBase& tracking_events,bool flush_display = true);

    typedef WaitingForInput initial_state;      //initial state of our state machine tracker

    //Guards
    bool input_selected(input_ready const& evt);
    bool no_input_selected(input_ready const& evt);
    bool flashcode_detected(input_ready const& evt);
    bool flashcode_redetected(input_ready const& evt);
    bool model_detected(msm::front::none const&);
    bool mbt_success(input_ready const& evt);

    //actions
    void find_flashcode_pos(input_ready const& evt);
    void track_model(input_ready const& evt);
    statistics_t& get_statistics();

    struct transition_table : mpl::vector<
      //    Start               Event              Target                       Action                         Guard
      //   +------------------+--------------------+-----------------------+------------------------------+------------------------------+
      g_row< WaitingForInput  , input_ready        , WaitingForInput       ,                               &Tracker_::no_input_selected    >,
      //   +------------------+--------------------+-----------------------+------------------------------+------------------------------+
      g_row< WaitingForInput  , input_ready        , DetectFlashcode       ,                               &Tracker_::input_selected       >,
      //   +------------------+--------------------+-----------------------+------------------------------+------------------------------+
       _row< WaitingForInput  , select_input       , DetectFlashcode                                                                       >,
      //   +------------------+--------------------+-----------------------+------------------------------+------------------------------+
       _row< DetectFlashcode  , input_ready        , DetectFlashcode                                        /* default behaviour */        >,
      //   +------------------+--------------------+-----------------------+------------------------------+------------------------------+
        row< DetectFlashcode  , input_ready        , DetectModel           , &Tracker_::find_flashcode_pos,&Tracker_::flashcode_detected   >,
      //   +------------------+--------------------+-----------------------+------------------------------+------------------------------+
       _row< DetectModel      , msm::front::none   , DetectFlashcode                                          /* default behaviour */      >,
      //   +------------------+--------------------+-----------------------+------------------------------+------------------------------+
      g_row< DetectModel      , msm::front::none   , TrackModel            ,                               &Tracker_::model_detected       >,
      //   +------------------+--------------------+-----------------------+------------------------------+------------------------------+
       _row< TrackModel       , input_ready        , ReDetectFlashcode                                        /* default behaviour */      >,
      //   +------------------+--------------------+-----------------------+------------------------------+------------------------------+
        row< TrackModel       , input_ready        , TrackModel            , &Tracker_::track_model       ,&Tracker_::mbt_success          >,
      //   +------------------+--------------------+-----------------------+------------------------------+------------------------------+
       _row< ReDetectFlashcode, input_ready        , DetectFlashcode                                        /* default behaviour */        >,
      //   +------------------+--------------------+-----------------------+------------------------------+------------------------------+
        row< ReDetectFlashcode, input_ready        , DetectModel           , &Tracker_::find_flashcode_pos,&Tracker_::flashcode_redetected >,
      //   +------------------+--------------------+-----------------------+------------------------------+------------------------------+
        row< ReDetectFlashcode, input_ready        , TrackModel            , &Tracker_::track_model       ,&Tracker_::mbt_success          >,
      //   +------------------+--------------------+-----------------------+------------------------------+------------------------------+
       _row< TrackModel       , finished           , Finished                                                                              >,
      //   +------------------+--------------------+-----------------------+------------------------------+------------------------------+
       _row< DetectModel      , finished           , Finished                                                                              >,
      //   +------------------+--------------------+-----------------------+------------------------------+------------------------------+
       _row< DetectFlashcode  , finished           , Finished                                                                              >,
      //   +------------------+--------------------+-----------------------+------------------------------+------------------------------+
       _row< ReDetectFlashcode, finished           , Finished                                                                              >
      //   +------------------+--------------------+-----------------------+------------------------------+------------------------------+
      > {};

  };

  typedef msm::back::state_machine<Tracker_> Tracker;


}
#endif //__TRACKING_H__
