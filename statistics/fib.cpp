#include <iostream>
#include <tbb/task.h>
#include <tbb/task_scheduler_init.h>

using namespace tbb;
using namespace std;

struct FibTask: public task
{
	int n;
	long *sum;

	FibTask(int n_, long *sum_): n(n_), sum(sum_) { }

	task* execute() {
		if (n < 2) {
			*sum = 1;
		} else {
			long x, y;

			FibTask &a = *new(allocate_child()) FibTask(n - 1, &x);
			FibTask &b = *new(allocate_child()) FibTask(n - 2, &y);

			set_ref_count(3);

			spawn(a);
			spawn_and_wait_for_all(b);

			*sum = x + y;
		}

		return NULL;
	}
};

long ParallelFib(long n) {
}

int main(int argc, char* argv[])
{
	if (argc != 3) {
		cout << "Usage: " << argv[0] << " <N> <number_of_iterations>" << endl;
		cout << "   Calculates N-th Fibonacci number number_of_iterations times" << endl;
		return 0;
	}

	int n = strtol(argv[1], 0, 0);
	int iter = strtol(argv[2], 0, 0);

	task_scheduler_init scheduler_init(2, 256); // 2 deques with 256 elements
	
	long sum;
	for (int i = 0; i < iter; i++) {
		FibTask &t = *new(task::allocate_root()) FibTask(n, &sum);
		task::spawn_root_and_wait(t);
	}

	return 0;
}
