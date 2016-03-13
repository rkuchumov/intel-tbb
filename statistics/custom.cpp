#include <iostream>
#include <tbb/task.h>
#include <tbb/task_scheduler_init.h>

using namespace tbb;
using namespace std;

struct CustomTask: public task
{
	int depth;
	int breadth;

	CustomTask(int depth_, int breadth_): depth(depth_), breadth(breadth_) {}

	task *execute() {
		// TDOD: actually calculate something
		
		if (depth < 1)
			return NULL;

		set_ref_count(breadth + 1);

		for (int i = 0; i < breadth; i++) {
			CustomTask &t = *new(allocate_child()) CustomTask(depth - 1, breadth);
			spawn(t);
		}

		wait_for_all();

		return NULL;
	}
};

int main(int argc, char* argv[])
{
	if (argc != 4) {
		cout << "Usage: " << argv[0] << " <Depth> <Breadth> <number_of_iterations>" << endl;
		cout << "   Calculates custom task graph with specified depth and breadth" << endl;
		return 0;
	}

	int depth = strtol(argv[1], 0, 0);
	int breadth = strtol(argv[2], 0, 0);
	int iter = strtol(argv[3], 0, 0);

	task_scheduler_init scheduler_init(2, 256); // 2 deques with 256 elements
	
	for (int i = 0; i < iter; i++) {
		CustomTask &t = *new(task::allocate_root()) CustomTask(depth, breadth);
		task::spawn_root_and_wait(t);
	}

	return 0;
}
