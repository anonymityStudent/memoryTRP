#n

#multi-line delete and read

/^\.\.@ COMPANY LIST/r ../inputs/company.list
/^\.\.@/{
        N
        P
        D
}
