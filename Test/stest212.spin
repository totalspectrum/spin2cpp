PUB demo
  outa := answer(4)
  outb := answer(2)

pub answer(question)
  asm
          mov     result, question        ' result as target
          add     result, result          ' result as source too
          add     result, question
  endasm
