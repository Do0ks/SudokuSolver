# Sudoku Puzzle Solver

![SSolver](https://github.com/user-attachments/assets/21945507-7c00-40ef-880c-18c51b4660c6)

<details>
  <summary>Video</summary>
  https://github.com/user-attachments/assets/bc78bca8-2866-4024-8f4c-813ba12705c2
</details>

<b><u>Discription:</u></b>

Sudoku Puzzle solver that handles mutiple grids plus puzzles that are 1-9/A-Z. In simplest words, it "brute forces" the puzzle. The harder the puzzle is, the longer it will take to solve.

<b>Backtracking Algorithm:</b>

The algorithm checks all possible candidates, that it be digits or letters, alligns a candidate and moves to the next cell. If a conflict is found, it backtracks and trys the next candidate. I also have functions inplemented that checks to make sure a voliation of the sudoku rule set doesnt happen before placing the candidate. 
