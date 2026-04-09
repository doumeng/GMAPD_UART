#ifndef _IMG_DATA_QUEUE_H_
#define _IMG_DATA_QUEUE_H_

#include <iostream>
#include <mutex>
#include <queue>
#include <list>
#include "commInfo.h"
#ifdef __cplusplus
extern "C" {
#endif

class img_data_queue
{
private:
    std::queue <IMG_SRC_INFO_S> img_queue;
    std::mutex *img_queue_mutex;
    size_t max_frame_num;

public:
    img_data_queue(/* args */);
    img_data_queue(int count);
    ~img_data_queue();

    void img_push_to_queue(IMG_SRC_INFO_S src);

    void img_queue_set_max_frame_num(size_t count);

    void img_queue_pop(IMG_SRC_INFO_S &src);

    bool img_get_queue_frist(IMG_SRC_INFO_S *imgSrc);
};


#ifdef __cplusplus
}
#endif

#endif // !_IMG_DATA_QUEUE_H_