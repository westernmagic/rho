expected <- eval(parse(text="character(0)"));      
test(id=0, code={      
argv <- eval(parse(text="list(integer(0), TRUE, NULL, 0L, NULL, 3L, TRUE, NA)"));      
.Internal(`format`(argv[[1]], argv[[2]], argv[[3]], argv[[4]], argv[[5]], argv[[6]], argv[[7]], argv[[8]]));      
}, o=expected);      

