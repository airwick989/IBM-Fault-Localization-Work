// eslint-disable-next-line
import { Link } from 'react-router-dom';
import React, { useState } from "react";

const Home = () => {

    const [file1, setFile1] = useState("");
    const [file2, setFile2] = useState("");


    const handleSubmit = (e) => {
        e.preventDefault();
        const dataSent = { file1, file2 };
        console.log(dataSent)
    }


    return ( 
        <div className="Home">

            <div class="hero min-h-screen bg-base-200">
            <div class="hero-content text-center">

                <form onSubmit={handleSubmit}>

                    <div class="max-w-md">
                        <h1 class="text-5xl font-bold">Enter Your Files Here</h1>
                        <p class="py-6">Provident cupiditate voluptatem et in. Quaerat fugiat ut assumenda excepturi exercitationem quasi. In deleniti eaque aut repudiandae et a id nisi.</p>
                        
                        <label class="label">
                            <span class="label-text">File 1</span>
                        </label>
                        <input type="file" 
                            class="file-input file-input-bordered file-input-accent w-full max-w-xs" 
                            style={{marginBottom: 50}}
                            value={file1}
                            name="file1"
                            required
                            onChange={e => setFile1(e.target.value)}
                        />

                        <label class="label">
                            <span class="label-text">File 2</span>
                        </label>
                        <input type="file" 
                            class="file-input file-input-bordered file-input-accent w-full max-w-xs" 
                            style={{marginBottom: 50}}
                            value={file2}
                            name="file2"
                            required
                            onChange={e => setFile2(e.target.value)}
                        />
                        
                        <button className='btn btn-primary' type='submit'>Send to Classifier</button>
                    </div>

                </form>


            </div>
            </div>

        </div>
    );
}

export default Home;