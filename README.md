# Intel(R) Threading Building Blocks with modified task scheduler to allow for deque operations analysis.

Right now scheduler with only 2 workers is supported. Before executing any operation with deques (push, pop, steal) workers specify it by setting flags in shared memory. When this operation is finished corresponding flags are also changed. Statistics is collected by the third thread that checks in infinite loop what workers are doing.

Statistics inforamtion includes:
* the number of iterations of statistic thread
* average number of deque operations per iteration
* the number of concurrent executions of `push`, `pop` and `noop` and their probabilities
* total number of `push`, `pop` and `steal` operations

For example, `./statistics/fib 25 10` calculates 25-th Fibonacci number 10 times using recurrence formula and produces following output:

```
====================== Deques Stats =====================

iterations: 12145760
operations per iteration:
th0: 0.10271  th1: 0.0971817

push-noop (p1):  	     181821	0.014970038680483
noop-push (p2):  	     173089	0.014251098746383
push-push (p12): 	       9873	0.000812882955722
pop-noop (q1):   	      21538	0.001773308325772
noop-pop (q2):   	      89426	0.007362794611409
pop-pop (q12):   	        214	0.000017619462425
push-pop (pq21): 	       1812	0.000149189093059
pop-push (qp12): 	        424	0.000034909589104
noop-noop (r):   	   11667463	0.960628158535642

Thread 0: 
push: 	623749
pop: 	623671
steal: 	74

Thread 1: 
push: 	590171
pop: 	590097
steal: 	78

Total: 
push: 	1213920
pop: 	1213768
steal: 	152
```

## Installation
1.  Compile Intel TBB by running `make`
2.  In `statistics` directory there is 2 test programs that you can compile and run with `make run`
