#include "rosbag/bag_player.h"

namespace rosbag {

BagPlayer::BagPlayer(const std::string &fname) {
    bag.open(fname, rosbag::bagmode::Read);
    orbbecRosbag::Time::init();
    View v(bag);
    bag_start_         = v.getBeginTime();
    bag_end_           = v.getEndTime();
    last_message_time_ = orbbecRosbag::Time(0);
    playback_speed_    = 1.0;
}

BagPlayer::~BagPlayer() {
    bag.close();
}

orbbecRosbag::Time BagPlayer::get_time() {
    return last_message_time_;
}

void BagPlayer::set_start(const orbbecRosbag::Time &start) {
    bag_start_ = start;
}

void BagPlayer::set_end(const orbbecRosbag::Time &end) {
    bag_end_ = end;
}

void BagPlayer::set_playback_speed(double scale) {
    if(scale > 0.0)
        playback_speed_ = scale;
}

orbbecRosbag::Time BagPlayer::real_time(const orbbecRosbag::Time &msg_time) {
    return play_start_ + (msg_time - bag_start_) * (1 / playback_speed_);
}

void BagPlayer::start_play() {

    std::vector<std::string> topics;
    for(auto const &cb: cbs_)
        topics.push_back(cb.first);

    View view(bag, TopicQuery(topics), bag_start_, bag_end_);
    play_start_ = orbbecRosbag::Time::now();

    for(auto const &m: view) {
        if(cbs_.find(m.getTopic()) == cbs_.end())
            continue;

        orbbecRosbag::Time::sleepUntil(real_time(m.getTime()));

        last_message_time_ = m.getTime(); /* this is the recorded time */
        cbs_[m.getTopic()]->call(m);
    }
}

void BagPlayer::unregister_callback(const std::string &topic) {
    delete cbs_[topic];
    cbs_.erase(topic);
}

}  // namespace rosbag
