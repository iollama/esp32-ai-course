from fastapi import FastAPI, HTTPException
from datetime import datetime
from pydantic import BaseModel
from typing import Optional

app = FastAPI()

class Student(BaseModel):
    fname: Optional[str] = None
    lname: Optional[str] = None
    phone: Optional[str] = None
    email: Optional[str] = None

# Initial student data
INITIAL_STUDENT_DATA = """fname	lname	phone	email
Yossi	Tzemach	0523529455	etgarss@gmail.com
Leon	Koll	0545660822	napobo3@gmail.com
Nimrod	Segev	0538246474	nimrod.segev@gmail.com
Taya	Fahmi	0559653110	Tayacomp@gmail.com
Ari	Shimony	0542009878	ari20040@gmail.com
Ami	Braun	0544462007	braun.ami@live.com
Shmulik	Schwartz	0547584133	shmulick.hadarim@gmail.com
Oren	Eilam	0546315514	oreniailam@gmail.com
Gabriel	Rozik	0508811359	Rozicgabi@gmail.com
"""

STUDENTS_FILE = "students.tsv"

@app.get("/course")
async def make_lab_course_default():
    return {"response": "hello hello"}

@app.get("/course/time")
async def make_lab_course_time():
    current_time = datetime.now().strftime("%H:%M")
    return {"response": current_time}

@app.get("/course/date")
async def make_lab_course_date():
    current_date = datetime.now().strftime("%B %d, %Y")
    return {"response": current_date}

@app.post("/course/resettable")
async def reset_table():
    try:
        with open(STUDENTS_FILE, 'w', encoding='utf-8') as f:
            f.write(INITIAL_STUDENT_DATA)
        return {"status": "success", "message": "Students table reset to initial data"}
    except Exception as e:
        return {"status": "error", "message": str(e)}

@app.get("/course/student/{number}")
async def get_student(number: int):
    try:
        with open(STUDENTS_FILE, 'r', encoding='utf-8') as f:
            lines = f.readlines()

        # Line 0 is the header, student data starts at line 1
        # So line number 1 corresponds to index 1 in the list
        if number < 1 or number >= len(lines):
            raise HTTPException(status_code=404, detail=f"Student at line {number} not found")

        # Parse the header to get field names
        header = lines[0].strip().split('\t')
        # Parse the student data
        student_data = lines[number].strip().split('\t')

        # Create a dictionary with field names as keys
        student = {header[i]: student_data[i] for i in range(len(header))}

        return student
    except FileNotFoundError:
        raise HTTPException(status_code=404, detail="Students file not found")
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/course/student")
async def add_student(student: Student):
    try:
        # Check if at least one field is provided
        if not any([student.fname, student.lname, student.phone, student.email]):
            raise HTTPException(status_code=400, detail="At least one student field must be provided")

        # Fill in NONE for missing fields
        fname = student.fname if student.fname else "NONE"
        lname = student.lname if student.lname else "NONE"
        phone = student.phone if student.phone else "NONE"
        email = student.email if student.email else "NONE"

        # Create the new line
        new_line = f"{fname}\t{lname}\t{phone}\t{email}\n"

        # Append to the file
        with open(STUDENTS_FILE, 'a', encoding='utf-8') as f:
            f.write(new_line)

        return {"status": "success", "message": "Student added successfully", "student": {
            "fname": fname,
            "lname": lname,
            "phone": phone,
            "email": email
        }}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.delete("/course/student/{number}")
async def delete_student(number: int):
    try:
        with open(STUDENTS_FILE, 'r', encoding='utf-8') as f:
            lines = f.readlines()

        # Check if the line number is valid
        if number < 1 or number >= len(lines):
            raise HTTPException(status_code=404, detail=f"Student at line {number} not found")

        # Get the student data before deleting
        header = lines[0].strip().split('\t')
        student_data = lines[number].strip().split('\t')
        deleted_student = {header[i]: student_data[i] for i in range(len(header))}

        # Remove the line
        del lines[number]

        # Write back to the file
        with open(STUDENTS_FILE, 'w', encoding='utf-8') as f:
            f.writelines(lines)

        return {"status": "success", "message": f"Student at line {number} deleted successfully", "deleted_student": deleted_student}
    except FileNotFoundError:
        raise HTTPException(status_code=404, detail="Students file not found")
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/course/students")
async def get_all_students():
    try:
        with open(STUDENTS_FILE, 'r', encoding='utf-8') as f:
            lines = f.readlines()

        # Parse the header
        header = lines[0].strip().split('\t')

        # Parse all student data
        students = []
        for i in range(1, len(lines)):
            student_data = lines[i].strip().split('\t')
            if len(student_data) == len(header):  # Ensure valid line
                student = {header[j]: student_data[j] for j in range(len(header))}
                students.append(student)

        return {"count": len(students), "students": students}
    except FileNotFoundError:
        raise HTTPException(status_code=404, detail="Students file not found")
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.put("/course/student/{number}")
async def update_student(number: int, student: Student):
    try:
        # Check if at least one field is provided
        if not any([student.fname, student.lname, student.phone, student.email]):
            raise HTTPException(status_code=400, detail="At least one field must be provided to update")

        with open(STUDENTS_FILE, 'r', encoding='utf-8') as f:
            lines = f.readlines()

        # Check if the line number is valid
        if number < 1 or number >= len(lines):
            raise HTTPException(status_code=404, detail=f"Student at line {number} not found")

        # Parse the header to get field names
        header = lines[0].strip().split('\t')
        # Parse the existing student data
        student_data = lines[number].strip().split('\t')

        # Create current student dict
        current_student = {header[i]: student_data[i] for i in range(len(header))}

        # Update only the provided fields
        if student.fname is not None:
            current_student['fname'] = student.fname
        if student.lname is not None:
            current_student['lname'] = student.lname
        if student.phone is not None:
            current_student['phone'] = student.phone
        if student.email is not None:
            current_student['email'] = student.email

        # Reconstruct the line
        updated_line = f"{current_student['fname']}\t{current_student['lname']}\t{current_student['phone']}\t{current_student['email']}\n"
        lines[number] = updated_line

        # Write back to the file
        with open(STUDENTS_FILE, 'w', encoding='utf-8') as f:
            f.writelines(lines)

        return {"status": "success", "message": f"Student at line {number} updated successfully", "updated_student": current_student}
    except FileNotFoundError:
        raise HTTPException(status_code=404, detail="Students file not found")
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
