// eslint-disable-next-line
import { Link } from 'react-router-dom';
import React, { useState } from "react";

const Home = () => {

    const [fileJLM, setFileJLM] = useState("");
    const [filePerf, setFilePerf] = useState("");
    const [fileTest, setFileTest] = useState("");


    const handleSubmit = (e) => {
        e.preventDefault();
        const dataSent = { fileJLM, filePerf, fileTest };
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
                            <span class="label-text">JLM Data CSV</span>
                        </label>
                        <input type="file" 
                            class="file-input file-input-bordered file-input-accent w-full max-w-xs" 
                            style={{marginBottom: 50}}
                            value={fileJLM}
                            name="fileJLM"
                            required
                            onChange={e => setFileJLM(e.target.value)}
                        />

                        <label class="label">
                            <span class="label-text">Perf Data CSV</span>
                        </label>
                        <input type="file" 
                            class="file-input file-input-bordered file-input-accent w-full max-w-xs" 
                            style={{marginBottom: 50}}
                            value={filePerf}
                            name="filePerf"
                            required
                            onChange={e => setFilePerf(e.target.value)}
                        />

                        <label class="label">
                            <span class="label-text">Test Data CSV</span>
                        </label>
                        <input type="file" 
                            class="file-input file-input-bordered file-input-accent w-full max-w-xs" 
                            style={{marginBottom: 50}}
                            value={fileTest}
                            name="fileTest"
                            required
                            onChange={e => setFileTest(e.target.value)}
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