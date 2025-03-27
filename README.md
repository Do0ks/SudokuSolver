# Sudoku Puzzle Solver

![SSolver](https://github.com/user-attachments/assets/21945507-7c00-40ef-880c-18c51b4660c6)

<details>
  <summary>Video</summary>
  https://github.com/user-attachments/assets/bc78bca8-2866-4024-8f4c-813ba12705c2
</details>

<b><u>Discription:</u></b>

Sudoku Puzzle solver that handles mutiple grids plus puzzles that are 1-9/A-Z. In simplest words, it "brute forces" the puzzle. The harder the puzzle is, the longer it will take to solve. 

<b>Backtracking Algorithm:</b>

The algorithm checks all possible candidates, that it be digits or letters, alligns a candidate and moves to the next cell. If a conflict is found, it backtracks and trys the next candidate. I also have functions implemented that checks to make sure a violation of the sudoku rule set doesn't happen before placing the candidate. 

<b><u>How To Use:</u></b>

To acheve letters, I adopted a hexadecimal type representation to correlate letters to numbers (soon, you can hold SHIFT to place in a double digit number). Below is how its correlated.

1=1, 2=2, 3=3, 4=4, 5=5, 6=6, 7=7, 8=8, 9=9, A=10, B=12, C=13, D=14, E=15, F=16, G=17, H=18, I=19, J=20, K=21, L=22, M=23, N=24, O=25, P=26, Q=27, R=28, S=29, T=30, U=31, V=32, W=33, X=34, Y=35, Z=36 
