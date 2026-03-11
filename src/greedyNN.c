//TODO: Check:_tour must provide a cost of the solution that must be consistent.
//TODO: In the command line you have to provide the time limit. 
//  (When you implement the function you have to compare the current time with the time limit provided)
//  (To do tihs you have to store the starting time that must be internal to the instance)
//  (At the end of the function you must compare the actual time computed with the time limit provided)
//  In every function that perform a long computation you most do this comparison between the time limit and the actual computed time

// NOTE: To compare 2 double numbers the best way is to compare the abs difference between the 2 numbers with an epsilon that you have defined

//NOTE: If you see that you encounter a time limit while you are computing the solution you can:
//  randomize with swaps a given istance, to start from different nodes to make different trials to avoid the time limit
//  if you have already a random solution than it's enough.

//NOTE: You check the time limit when you change the starting node (so outsite the function that implements the NN solution)