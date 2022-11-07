import { Link } from 'react-router-dom';

const Home = () => {
    return ( 
        <div className="Home">

            <div class="hero min-h-screen bg-base-200">
            <div class="hero-content text-center">
                <div class="max-w-md">
                <h1 class="text-5xl font-bold">Hello there</h1>
                <p class="py-6">Provident cupiditate voluptatem et in. Quaerat fugiat ut assumenda excepturi exercitationem quasi. In deleniti eaque aut repudiandae et a id nisi.</p>
                <Link to="/classifier">
                    <button className='btn btn-primary'>Go to Classifier</button>
                </Link>
                </div>
            </div>
            </div>

        </div>
    );
}

export default Home;