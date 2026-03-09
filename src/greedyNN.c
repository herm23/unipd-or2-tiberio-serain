//TODO: Organize the directories and files in the correct way by also checking the CMakeLists.txt !!!
//TODO: Check:_tour must provide a cost of the solution that must be consistent.
//TODO: In the command line you have to provide the time limit. 
//  (When you implement the function you have to compare the current time with the time limit provided)
//  (To do tihs you have to store the starting time that must be internal to the instance (check this istance what it is))
//  (At the end of the function you must compare the actual time computed with the time limit provided)
//  In every function that perform a long computation you most do this comparison between the time limit and the actual computed time
//  NOTE: The incombent musst be implemented by a single function

// NOTE: To compare 2 double numbers the best way is to compare the abs difference between the 2 numbers with an epsilon that you have defined

//TODO: We have to implement the TSP solution with the NN heuristics approach. 
//(On input you must provide the starting nodes sequence.)
//Steps:
//Init a vector with the all nodes
//Chose the starting node at random by performing the swap with (first node in the vector and actual chosen first node)
//Chose the next node based on the lowest cost from the first visited node and all the other nodes
//Iterate this procedure until you have visited the all nodes

//NOTE: If you see that you encounter a time limit while you are computing the solution you can:
//  randomize with swaps a given istance, to start from different nodes to make different trials to avoid the time limit
//  if you have already a random solution than it's enough.

//NOTE: You check the time limit when you change the starting node (so outsite the function that implements the NN solution)